-- 1. Setup domains to prevent floating-point math errors
CREATE DOMAIN monetary_amount AS NUMERIC(19, 4) CHECK (VALUE >= 0);

-- 2. The Core Double-Entry ACID Stored Procedure
CREATE OR REPLACE FUNCTION process_financial_transfer(
    p_idempotency_key UUID,
    p_source_account_id BIGINT,
    p_destination_account_id BIGINT,
    p_transfer_amount monetary_amount,
    p_currency_code CHAR(3)
) RETURNS BOOLEAN
LANGUAGE plpgsql
AS $$
DECLARE
    v_source_balance monetary_amount;
    v_dest_balance monetary_amount;
    v_first_lock BIGINT;
    v_second_lock BIGINT;
BEGIN
    -- 1. Idempotency Check: Ensure this exact transaction hasn't happened before
    -- If the network dropped and the client retired, we safely ignore the duplicate
    IF EXISTS (SELECT 1 FROM processed_transactions WHERE idempotency_key = p_idempotency_key)
        RAISE NOTICE 'Transaction % already processed. Bypassing execution.', p_idempotency_key;
        RETURN TRUE;
    END IF;

    -- 2. Deadlock Prevention Strategy
    -- We MUST lock the rows in a consistent order (smallest ID first) across all concurrent threads
    IF p_source_account_id < p_destination_account_id THEN
        v_first_lock := p_source_account_id;
        v_second_lock := p_destination_account_id;
    ELSE
        v_first_lock := p_destination_account_id;
        v_second_lock := p_source_account_id;
    END IF;

    -- 3. The Row-Level Locks (Isolation & Consistency)
    -- This physically blocks any other server threads from touching these accounts
    PERFORM 1 FROM accounts WHERE account_id = v_first_lock FOR UPDATE;
    PERFORM 1 FROM accounts WHERE account_id = v_second_lock FOR UPDATE;

    -- 4. Re-query the current state AFTER acquiring the locks
    SELECT balance, currency INTO v_source_balance FROM accounts WHERE account_id = p_source_account_id;

    -- Business Logic / Constraints Check
    IF v_source_balance < p_transfer_amount THEN
        RAISE EXCEPTION 'Insufficient funds in source account %', p_source_account_id
            USING ERRCODE = 'check_violation';
    END IF;

    -- 5. The Double-Entry Ledger Insert (Atomicity)
    -- Both inserts must succeeed, or the entire block is rolled back by the database engine
    INSERT INTO ledger_entries (transaction_id, account_id, entry_type, amount, currency, created_at)
    VALUES
        (p_idempotency_key, p_source_account_id, 'DEBIT', p_transfer_amount, p_currency_code, NOW()),
        (p_idempotency_key, p_destination_account_id, 'CREDIT', p_transfer_amount, p_currency_code, NOW());

    -- 6. Materialize the View (Update the chaced balance)
    UPDATE accounts SET balance = balance - p_transfer_amount, last_updated = NOW()
    WHERE account_id = p_source_account_id;

    UPDATE accounts SET balance = balance + p_transfer_amount, last_updated = NOW()
    WHERE account_id = p_destination_account_id;

    -- 7. Record the successful idempotency key
    INSERT INTO processed_transactions (idempotency_key, created_at)
    VALUES (p_idempotency_key, NOW());

    RETURN TRUE;

EXCEPTION
    -- Dynamic Exception Trapping and Rollback
    WHEN check_violation THEN
        -- The transaction is automatically aborted, and locks are released
        INSERT INTO failed_transactions (idempotency_key, error_reason, failed_at)
        VALUES (p_idempotency_key, SQLERRM, NOW());
        RETURN FALSE;
    WHEN OTHERS THEN
        -- Catch-all for database hardware failures or severe constraints
        RAISE WARNING 'Critical Ledger Failure: %', SQLERRM;
        RETURN FALSE;
END;
$$;
