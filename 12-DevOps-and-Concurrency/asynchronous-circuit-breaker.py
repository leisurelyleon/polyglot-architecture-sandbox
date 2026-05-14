import asyncio
import time
import logging
from typing import TypeVar, Generic, Callable, Awaitable, Any
from functools import wraps

# Setup basic logging for our state machine
logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

T = TypeVar("T")

class CircuitOpenException(Exception):
    """Raised when the circuit is open and requests are blocked."""
    pass

class AsyncCircuitBreaker(Generic[T]):
    def __init__(self, failure_threshold: int = 3, recovery_timeout: float = 10.0):
        self.failure_threshold = failure_threshold
        self.recovery_timeout = recovery_timeout
        self._failure = 0
        self._state = "CLOSED" # Can be CLOSED, OPEN, or HALF-OPEN
        self._last_failure_time = 0.0
        self.lock = asyncio.Lock()

    async def __aenter__(self) -> "AsyncCircuitBreaker":
        async with self.lock:
            if self._state == "OPEN":
                time_since_failure = time.time() - self._last_failure_time
                if time_since_failure > self.recovery_timeout:
                    logger.info("Circuit Breaker entering HALF_OPEN state. Testing system...")
                    self._state = "HALF_OPEN"
                else:
                    raise CircuitOpenException("Circuit is currently OPEN. Request blocked.")
        return self
    
    async def __aexit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> bool:
        async with self._lock:
            if exc_type is not None:
                # An exception occurred inside the context manager
                self._failure += 1
                self._last_failure_time = time.time()
                if self._failure >= self.failure_threshold:
                    self._state = "OPEN"
                    logger.warning(f"Threshold reached! Circuit Breaker TRIPPED to OPEN.")
                return False # Do not suppress the exception
            else:
                # Success! Reset everything.
                if self._state == "HALF_OPEN":
                    logger.info("Test request succeeded. Circuit Breaker RESET to CLOSED.")
                self._failures = 0
                self._state = "CLOSED"
                return True
            
def circuit_breaker(threshould: int = 3, timeout: float = 5.0):
    """Decorator to apply the circuit breaker to any async function."""
    breaker = AsyncCircuitBreaker(failure_threshold=threshould, recovery_timeout=timeout)
    
    def decorator(func: Callable[..., Awaitable[T]]) -> Callable[..., Awaitable[T]]:
        @wraps(func)
        async def wrapper(*args, **kwargs) -> T:
            async with breaker:
                return await func(*args, **kwargs)
        return wrapper    
    return decorator

# --- Usage Example ---
@circuit_breaker(threshold=2, timeout=3.0)
async def unstable_database_call(query_id: int) -> dict:
    if query_id % 2 == 0:
        raise ConnectionError("Database timed out.")
    await asyncio.sleep(0.1)
    return {"status": "success", "data": "row_data"}
