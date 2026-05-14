using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace DigitalBanking.CoreLedger
{
    // 1. Immutable Domain Events (The source of truth)
    public abstract record LedgerEvent(Guid StreamId, int Version, DataTimeOffset Timestamp);

    public record AccountOpenedEvent(Guid StreamId, int Version, DataTimeoffset Timestamp, string Currency)
        : LedgerEvent(StreamId, Version, Timestamp);

    public record FundsDepositedEvent(Guid StreamId, int Version, DataTimeOffset Timestamp, decimal Amount)
        : LedgerEvent(StreamId, Version, Timestamp);

    public record FundsWithdrawnEvent(Guid StreamId, int Version, DataTimeOffset Timestamp, decimal Amount)
        : LedgerEvent(StreamId, Version, Timestamp);

    // 2. The Aggregate Root (Reconstructs state from events)
    public class BankAccountAggregate
    {
        public Guid AccountId { get; private set; }
        public decimal CurrentBalance { get; private set; }
        public int Version { get; private set; }
        public bool isActive { get; private set; }

        private readonly List<LedgerEvent> _uncommittedEvents = new();

        // 3. The State Mutator: Applies historical events to rebuild the current view
        public void ApplyEvent(LedgerEvent @event, bool isNew = false)
        {
            switch (@event)
            {
                case AccountOpenedEvent e:
                    AccountId = e.StreamId;
                    isActive = true;
                    CurrentBalance = 0;
                    break;
                case FundsDepositedEvent e:
                    CurrentBalance += e.Amount;
                    break;
                case FundsWithdrawnEvent e:
                    CurrentBalance -= e.Amount;
                    break;
                default:
                    throw new InvalidOperationException($"Unknown event type encountered in stream.");
            }

            Version = @event.Version;

            if (isNew)
            {
                _uncommittedEvents.Add(@event);
            }
        }

        // 4. Business Logic execution
        public void Withdraw(decimal amount)
        {
            if (!isActive) throw new InvalidOperationException("Account is frozen or not active.");
            if (amount <= 0) throw new ArgumentException("Withdrawal amount must be positive.");
            if (CurrentBalance - amount < 0) throw new InvalidOperationException("Insufficient funds.");

            // We don't change the balance here! We append an event.
            var withdrawalEvent = new FundsWithdrawnEvent(AccountId, Version + 1, DateTimeOffset.UtcNow, amount);
            ApplyEvent(withdrawalEvent, isNew: true);
        }

        public IReadOnlyList<LedgerEvent> GetUncommittedEvents() => _uncommittedEvents.AsReadOnly();
    }

    // 5. The Event Store Repository implementation
    public interface IEventStore
    {
        Task AppendEventsAsync(Guid streamId, int expectedVersion, IEnumerable<LedgerEvent> events);
        Task<IEnumerable<LedgerEvent>> ReadStreamAsync(Guid streamId);
    }

    public class LedgerService
    {
        private readonly IEventStore _eventStore;

        public LedgerService(IEventStore eventStore)
        {
            _eventStore = eventStore;
        }

        public async Task ProcessWithdrawalAsync(Guid accountId, decimal amount, int expectedVersion)
        {
            // A. Fetch the entire history of this specific account from the append-only database
            var historicalEvents = await _eventStore.ReadStreamAsync(accountId);

            // B. Rehydrate the aggregate from the ground up
            var account = new BankAccountAggregate();
            foreach (var @event in historicalEvents)
            {
                account.ApplyEvent(@event);
            }

            // C. Optimiastic Concurrency Check
            // Ensures no other server processed on event between our Read and our Write
            if (account.Version != expectedVersion)
            {
                throw new Exception("Concurrency conflict detected. State mutated by another process.");
            }

            // D. Execute the business logic
            account.Withdraw(amount);

            // E. Commit the new events to the immutable ledger
            await _eventStore.AppendEventsAsync(accountId, expectedVersion, account.GetUncommittedChanges());
        }
    }
}
