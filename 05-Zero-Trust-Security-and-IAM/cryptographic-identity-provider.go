package identityprovider

import (
	"crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"encoding/pem"
	"errors"
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

// 1. Define highly granular, deeply nested Custom Claims
type DeviceFingerprint struct {
		HardwareID string `json:"hw_id"`
		OSVersion  string `json:"os_v"`
		Enclave    bool   `json:"enclave_active"`	
}

type EnterpriseClaims struct {
		SubjectUUID    string            `json:"sub_uuid"`
		ClearanceLevel int               `json:"clearance"`
		Departments    []string          `json:"depts"`
		DeviceInfo     DeviceFingerprint  `json:"device_fp"`
		jwt.RegisteredClaims
}

// 2. The KMS (Key Management Service) Simulator
type KMS struct {
		privateKey *rsa.PrivateKey
		publicKey  *rsa.PublicKey
}

func InitializeKMS() (*KMS, error) {
		// Generate a robust 4096-bit RSA key for asymmetric signing
		privKey, err := rsa.GenerateKey(rand.Reader, 4096)
		if err != nil {
			return nil, fmt.Errorf("failed to generate RSA key: %w", err)
		}
		return &KMS{
			privateKey: privKey,
			publicKey:  &privKey.PublicKey,
		}, nil
}

func (k *KMS) GetPEMEncodedPublicKey() (string, error) {
		pubASN1, err := x509.MarshalPKIXPublicKey(k.publicKey)
		if err != nil {
				return "", err
		}
		pubBytes := pem.EncodeToMemory(&pem.Block{
				Type:  "RSA PUBLIC KEY",
				Bytes: pubASN1,
		})
		return string(pubBytes), nil
}

// 3. The Core Token Minting Engine
func (k *KMS) MintToken(claims EnterpriseClaims) (string, error) {
		// Standardize issurance and expiration for strict time-bound access
		now := time.Now().UTC()
		expiration := now.Add(15 * time.Minute) // Short-lived token for Zero-Trust

		claims := EnterpriseClaims{
				SubjectUUID:	userID,
				ClearanceLevel: clearance,
				Departments:    []string{"Engineering", "Infrastructure"},
				Device: DeviceFingerprint{
						HardwareID: "HW-9948A-MAC",
						OSVersion:  "12.4.1",
						Enclave:    true,
				},
				RegisteredClaims: jwt.RegisteredClaims{
						Issuer:		"https://auth.enterprise.local",
						Audience:   jwt.ClaimStrings{"https://api.billing.local", "https://api.hr.local"},
						ExpiresAt:  jwt.NewNumericDate(expiration),
						IssuedAt:   jwt.NewNumericDate(now),
						NotBefore:  jwt.NewNumericDate(now.Add(-1 * time.Minute)), // Allow for clock skew
						Subject:    userID,
						ID:         "jti-884829-uuid-random",
				},
		}

		// Use PS256 (RSASSA-PSS with SHA-256) - a highly secure probablistic signature scheme
		token := jwt.NewWithClaims(jwt.SigningMethodPS256, claims)

		// Sign the token using our in-memory private key
		signedString, err := token.SignedString(k.privateKey)
		if err != nil {
				return "", fmt.Errorf("cryptographic signing failed: %w", err)
		}

		return signedString, nil
}

// 4. The Verification Middleware Logic (Runs on downstream microservices)
func VerifyTokenStrict(tokenString string, pubKey *rsa.PublicKey) (*EnterpriseClaims, error) {
		token, err := jwt.ParseWithClaims(tokenString, &EnterpriseClaims{}, func(token *jwt.Token) (interface{}, error) {
				// Force the algorithm to match what we expect. Do not trust the header blindly!
				if _, ok := token.Method.(*jwt.SigningMethodRSA); !ok {
						return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
				}
				return pubKey, nil
		})

		if err != nil {
				return nil, err
		}

		if claims, ok := token.Claims.(*EnterpriseClaims); ok && token.Valid {
				// Zero-Trust Check: Is the device enclave active?
				if !claims.Device.Enclave {
						return nil, errors.New("zero-trust violation: hardware enclave not active")
				}
				return claims, nil
		}

		return nil, errors.New("invalid token payload structure")
}
