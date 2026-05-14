package apigateway

import (
	"fmt"
	"net/http"
	"net/http/httputil"
	"net/url"
	"sync"
	"time"
)

// 1. The Token Bucket Structure
// Ensures precise rate limiting: e.g., 100 requests per second, with bursts up to 20
type TokenBucket struct {
		capacity   float64
		tokens	   float64
		refillRate float64 // tokens per second
		lastRefill time.Time
		mu		   sync.Mutex
}

func (tb *TokenBucket) Allow() bool {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	now := time.Now()
	// Calculate how many tokens to add based on the time elapsed
	timeElapsed := now.Sub(tb.lastRefill).Seconds()
	tokensToAdd := timeElapsed * tb.refillRate

	tb.tokens += tokensToAdd
	if tb.tokens > tb.capacity {
		tb.tokens = tb.capacity // Cap the bucket
	}
	tb.lastRefill = now

	// If we have at least 1 token, allow the request and consume the token
	if tb.tokens >= 1.0 {
		tb.tokens -= 1.0
		return true
	}
	return false
}

// 2. The Highly Concurrent Rate Limiter Engine
type RateLimiter struct {
	visitors sync.Map
	capacity float64
	rate     float64
}

func NewRateLimiter(rate float64, capacity float64) *RateLimiter {
	return &RateLimiter{
		capacity: capacity,
		rate:     rate,
	}
}

func (rl *RateLimiter) GetBucket(ip string) *TokenBucket {
	// sync.Map provides atomic, lock-free reads for high-throughput scaling
	bucket, exists := rl.visitors.Load(ip)
	if !exists {
		newBucket := &TokenBucket{
			capacity:   rl.capacity,
			tokens:     rl.capacity,
			refillRate: rl.rate,
			lastRefill: time.Now(),
		}
		// LoadOrStore ensures we don't overwrite if another goroutine just created it
		bucket, _ = rl.visitors.LoadOrStore(ip, newBucket)
	}
	return bucket.(*TokenBucket)
}

// 3. The Core API Gateway Reverse Proxy
func StartEdgeGateway(targetURL string, listenPort string) error {
	target, err := url.Parse(targetURL)
	if err != nil {
		return err
	}

	// Create a dynamic reverse proxy
	proxy := httputil.NewSingleHostReverseProxy(target)
	
	// Tweak the transport for high-throughput edge routing
	proxy.Transport = &http.Transport{
		MaxIdleConns:        10000,
		MaxIdleConnsPerHost: 1000,
		IdleConnTimeout:     90 * time.Second,
	}

	// 100 requests per second, max burst of 20
	limiter := NewRateLimiter(100, 20)

	// The Middleware Handler
	gatewayHandler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		clientIP := r.RemoteAddr // In production, parse X-Forwarded-For

		bucket := limiter.GetBucket(clientIP)
		if !bucket.Allow() {
			http.Error(w, "429 Too Many Requests - Rate Limit Exceeded", http.StatusTooManyRequests)
			return
		}

		// Inject tracing headers for downstream microservices
		r.Header.Set("X-Gateway-Trace-Id", generateTraceID())
		r.Header.Set("X-Gateway-Ingress-Time", fmt.Sprintf("%d", time.Now().UnixNano()))

		// Stream the request directly to the internal microservice without loading it into RAM
		proxy.ServeHTTP(w, r)
	})

	server := &http.Server{
		Addr:         ":" + listenPort,
		Handler:      gatewayHandler,
		ReadTimeout:  5 * time.Second, // Drop slowloris attacks immediately
		WriteTimeout: 10 * time.Second,
	}

	fmt.Printf("Edge Gateway running on port %s, routing to %s\n", listenPort, targetURL)
	return server.ListenAndServe()
}

// Helper stub for distributed tracing
func generateTraceID() string { return "trace-uuid-v4-mock" }
