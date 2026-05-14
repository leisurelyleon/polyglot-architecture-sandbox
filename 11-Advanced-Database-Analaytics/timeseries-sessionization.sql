CREATE TABLE raw_clickstream (
    event_id UUID PRIMARY KEY,
    user_id INT NOT NULL,
    event_timestamp TIMESTAMP NOT NULL,
    url_path VARCHAR(200) NOT NULL
);

-- 1. First Pass: Calculate the time delta between this event and the user's PREVIOUS event
WITH time_deltas AS (
    SELECT 
        user_id,
        event_timestamp,
        url_path,
        -- The LAG function looks backwards 1 row, partitioned strictly by the specific user, ordered by time
        EXTRACT(EPOCH FROM (
            event_timestamp - LAG(event_timestamp) OVER (
                PARTITION BY user_id 
                ORDER BY event_timestamp
            )
        )) / 60 AS minutes_since_last_action
    FROM raw_clickstream
),

-- 2. Second Pass: Flag the start of a new session
session_flags AS (
    SELECT 
        user_id,
        event_timestamp,
        url_path,
        minutes_since_last_action,
        -- If the gap is > 30 mins, or if it's their very first event (NULL), flag it as '1' (New Session)
        CASE 
            WHEN minutes_since_last_action IS NULL OR minutes_since_last_action > 30 THEN 1 
            ELSE 0 
        END AS is_new_session
    FROM time_deltas
),

-- 3. Third Pass: The Cumulative Sum trick to generate Unique Session IDs
sessionized_data AS (
    SELECT 
        user_id,
        event_timestamp,
        url_path,
        -- A running total of the flags. 
        -- Events 1, 2, 3 have flag 0 (Sum = 1). Event 4 has flag 1 (Sum = 2). 
        -- Thus, events 1-3 share Session ID 1, and event 4 starts Session ID 2!
        SUM(is_new_session) OVER (
            PARTITION BY user_id 
            ORDER BY event_timestamp 
            ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
        ) AS session_id_sequence
    FROM session_flags
)

-- 4. Final Aggregation: Treat each generated session as its own entity
SELECT 
    user_id,
    -- Create a globally unique session hash
    MD5(user_id::TEXT || '-' || session_id_sequence::TEXT) AS unique_session_id,
    MIN(event_timestamp) AS session_start_time,
    MAX(event_timestamp) AS session_end_time,
    COUNT(*) AS total_page_views,
    -- Extract the very first page they landed on
    FIRST_VALUE(url_path) OVER (
        PARTITION BY user_id, session_id_sequence 
        ORDER BY event_timestamp
    ) AS landing_page
FROM sessionized_data
GROUP BY user_id, session_id_sequence, landing_page
ORDER BY user_id, session_start_time;
