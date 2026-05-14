package distributedledger

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"
)

// 1. Defining the RPC Interfaces for external Bank Databases
type BankNode interface {
		Prepare(ctx context.Context, transactionID string, amount float64, operation string) error
		Commit(ctx context.Context, transactionID string) error
		Rollback(ctx context.Context, transactionID string) error
}

type TransactionCoordinator struct {
		SourceBank BankNode
		DestBank   BankNode
}

// 2. The Execution Engine for the 2PC Protocol
func (tc *TransactionCoordinator) ExecuteTransaction(txID string, amount float64) error {
		// A strictly enforced 5-second timeout for the ENTIRE distributed transaction
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()

		fmt.Printf("[TX %s] Initiating Two-Phase Commit...\n", txID)

		// --- PHASE 1: PREPARE ---
		// We must ask both databases concurrently if they can guarantee the transaction
		perpareErrChan := make(chan error, 2)
		var wg sync.WaitGroup

		wg.Add(2)
		go func() {
				defer wg.Done()
				// Source bank must promise it has the funds and lock them
				prepareErrChan <- tc.SourceBank.Prepare(ctx, txID, amount, "DEBIT")
		}()
		go func() {
				defer wg.Done()
				// Dest bank must promise it can accept the funds and lock the row
				prepareErrChan <- tc.DestBank.Prepare(ctx, txID, amount, "CREDIT")
		}()

		wg.Wait()
		close(prepareErrChan)

		// Evaluate Phase 1 Consensus
		phase1Success := true
		for err := range prepareErrChan {
				if err != nil {
						fmt.Printf("[TX %s] PREPARE failed: %v\n", txID, err)
						phase1Success = false
				}
		}

		// --- PHASE 2: RESOLUTION (COMMIT OR ROLLBACK) ---
		var finalWg sync.WaitGroup
		finalWg.Add(2)

		if !phase1Success {
				// If ANY node failed to prepare, we must globally abort and unlock resources
				fmt.Printf("[TX %s] Consensus failed. Triggering Global ROLLBACK.\n", txID)

				go func() { defer finalWg.Done(); tc.SourceBank.Rollback(context.Background(), txID) }()
				go func() { defer finalWg.Done(); tc.DestBank.Rollback(context.Background(), txID) }()

				finalWg.Wait()
				return errors.New("distributed transaction aborted due to prepare failure")				
		}

		// Both nodes voted YES. Proceed to global Commit.
		fmt.Printf("[TX %s] Phase 1 Consensus achieved. Triggering Global COMMIT.\n", txID)
		commitErrChan := make(chan error, 2)

		go func() {
				defer finalWg.Done()
				commitErrChan <- tc.SourceBank.Commit(context.Background(), txID)
		}()
		go func() {
				defer finalWg.Done()
				commitErrChan <- tc.DestBank.Commit(context.Background(), txID)
		}()

		finalWg.Wait()
		close(commitErrChan)

		// Evaluate Phase 2 Execution
		// If a commit fails here, the system is in an inconsistent state and requires human/sysadmin intervention (Heuristic Exception)
		for err := range commitErrChan {
				if err != nil {
						return fmt.Errorf("CRITICAL HEURISTIC EXCEPTION: Partial commit failure on TX %s: %v", txID, err)
				}
		}

		fmt.Printf("[TX %s] Two-Phase Commit completed successfully.\n", txID)
		return nil
}
