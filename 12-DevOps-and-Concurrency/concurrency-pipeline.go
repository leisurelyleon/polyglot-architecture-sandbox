package main

import (
	"context"
	"fmt"
	"math/rand"
	"sync"
	"time"
)

// Represents a complex unit of work
type Task struct {
	ID        int
	Data      int
	Processed bool
}

// 1. Fan-Out: Spawn multiple workers listening to the same input channel
func spawnWorkers(ctx context.Context, input <-chan Task, numWorkers int) []<-chan Task {
	workerChannels := make([]<-chan Task, numWorkers)

	for i := 0; i < numWorkers; i++ {
		out := make(chan Task)
		workerChannels[i] = out
		workerID := i

		go func(id int, in <-chan Task, output chan<- Task) {
			defer close(output)
			for {
				select {
				case <-ctx.Done():
					fmt.Printf("Worker %d halting due to context cancellation.\n", id)
					return
				case task, ok := <-in:
					if !ok {
						return // Input channel closed
					}
					// Simulate heavy backend processing
					time.Sleep(time.Duration(rand.Intn(100)) * time.Millisecond)
					task.Data = task.Data * 2
					task.Processed = true
					output <- task
				}
			}
		}(workerID, input, out)
	}
	return workerChannels
}

// 2. Fan-In: Multiplex multiple worker output channels into one unified stream
func fanIn(ctx context.Context, channels ...<-chan Task) <-chan Task {
	var wg sync.WaitGroup
	multiplexedStream := make(chan Task)

	// Helper function to read from a single channel and pipe to the unified stream
	multiplex := func(c <-chan Task) {
		defer wg.Done()
		for i := range c {
			select {
			case <-ctx.Done():
				return
			case multiplexedStream <- i:
			}
		}
	}

	// Add WaitGroup counters and start a goroutine for each channel
	wg.Add(len(channels))
	for _, c := range channels {
		go multiplex(c)
	}

	// Wait for all channels to drain, then close the main output stream
	go func() {
		wg.Wait()
		close(multiplexedStream)
	}()

	return multiplexedStream
}

func main() {
	// Setup timeout context to prevent dangling goroutines
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()

	inputStream := make(chan Task)

	// Start feeding data into the pipeline
	go func() {
		defer close(inputStream)
		for i := 1; i <= 20; i++ {
			select {
			case <-ctx.Done():
				return
			case inputStream <- Task{ID: i, Data: i * 10}:
			}
		}
	}()

	// Orchestrate the pipeline
	workerStreams := spawnWorkers(ctx, inputStream, 5) // 5 concurrent workers
	unifiedOutput := fanIn(ctx, workerStreams...)

	// Consume the final multiplexed results
	for result := range unifiedOutput {
		fmt.Printf("Processed Task %d: Resulting Data -> %d\n", result.ID, result.Data)
	}
}
