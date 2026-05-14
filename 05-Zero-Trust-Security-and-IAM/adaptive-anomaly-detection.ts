// 1. Domain Types and Interfaces
interface GeoLocation {
    latitude: number;
    longitude: number;
    city: string;
    region: string;
}

interface LoginAttempt {
    userId: string;
    ipAddress: string;
    userAgent: string;
    location: GeoLocation;
    timestamp: number;
    providedMfaToken?: string; // Standard TOTP
    providedWebAuthnSignature?: string; // FIDO2 Hardware Key
}

// 2. Custom Security Exceptions
class SecurityException extends Error {
    constructor(message: string, public readonly requiredAction: 'PROMPT_MFA' | 'PROMPT_WEBAUTHN' | 'ACCOUNT_LOCKED') {
        super(message);
        this.name = 'SecurityException';
    }
}

// 3. The Adaptive Risk Evaluator Engine
class AdaptiveRiskEngine {
    private readonly BASE_RISK_THRESHOLD = 50;
    private readonly CRITICAL_RISK_THRESHOLD = 85;

    // A mock in-memory store simulating a Redis cache for user history
    private userHistoryCache: Map<string, GeoLocation> = new Map();

    constructor() {
        // Pre-warm the cache with baseline known-good data
        this.userHistoryCache.set("usr_joseph_leon_001", {
            latitude: 37.6393,
            longitude: -120.9969,
            city: "Modesto",
            region: "CA"
        });
    }

    public async evaluateAuthentication(attempt: LoginAttempt): Promise<{ success: boolean; token?: string }> {
        let riskScore = 0;
        const baselineLocation = this.userHistoryCache.get(attempt.userId);

        // --- RISK CALCULATION HEURISTICS ---

        // Heuristic A: Geo-Velocity & Distance Anomaly
        if (baselineLocation) {
            if (baselineLocation.city !== attempt.location.city || baselineLocation.region !== attempt.location.region) {
                // User is suddenly logging in from outside their established Modesto/CA baseline
                riskScore += 40; 
            }
        } else {
            // Unrecognized user completely
            riskScore += 20; 
        }

        // Heuristic B: Device/UserAgent Anomaly
        if (attempt.userAgent.includes("Headless") || attempt.userAgent === "") {
            riskScore += 50; // Bots trying to script logins
        }

        console.log(`[Risk Engine] Calculated dynamic risk score for ${attempt.userId}: ${riskScore}`);

        // --- ADAPTIVE ENFORCEMENT ROUTING ---

        if (riskScore >= this.CRITICAL_RISK_THRESHOLD) {
            // Massive risk anomaly (e.g. Bot net from foreign IP). Force Hardware Key verification.
            if (!attempt.providedWebAuthnSignature) {
                throw new SecurityException(
                    "Critical risk detected. FIDO2 Hardware Security Key signature required.", 
                    'PROMPT_WEBAUTHN'
                );
            }
            this.verifyHardwareSignature(attempt.providedWebAuthnSignature);
            
        } else if (riskScore >= this.BASE_RISK_THRESHOLD) {
            // Moderate risk anomaly. Force standard 6-digit TOTP app.
            if (!attempt.providedMfaToken) {
                throw new SecurityException(
                    "Unusual login context detected. Authenticator app token required.", 
                    'PROMPT_MFA'
                );
            }
            this.verifyTotp(attempt.providedMfaToken);
        }

        // If we reach here, the risk was low, or the forced challenges were passed.
        this.updateBaseline(attempt);
        return { success: true, token: this.generateSession() };
    }

    // --- MOCK CRYPTOGRAPHIC VERIFIERS ---
    
    private verifyHardwareSignature(signature: string): void {
        // In reality, this verifies the CBOR-encoded ECDSA/RSA signature against the public key registered to the user
        if (signature !== "valid_fido2_sig_buffer") {
            throw new SecurityException("Invalid Hardware Key Signature. Account Locked.", 'ACCOUNT_LOCKED');
        }
    }

    private verifyTotp(token: string): void {
        // Time-based One Time Password validation algorithm (RFC 6238)
        if (token.length !== 6) {
            throw new SecurityException("Invalid TOTP Token Format.", 'PROMPT_MFA');
        }
    }

    private updateBaseline(attempt: LoginAttempt): void {
        // Shift the behavioral baseline based on successful secure logins
        this.userHistoryCache.set(attempt.userId, attempt.location);
    }

    private generateSession(): string {
        return "sess_abc123_secure_verified";
    }
}
