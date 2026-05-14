import org.apache.flink.api.common.state.{ValueState, ValueStateDescriptor}
import org.apache.flink.configuration.Configuration
import org.apache.flink.streaming.api.functions.KeyedProcessFunction
import org.apache.flink.streaming.api.scala._
import org.apache.flink.util.Collector

// 1. Define the immutable case classes representing our streaming data
case class TransactionEvent(accountId: String, amount: Double, timestamp: Long)
case class FraudAlert(accountId: String, reason: String, timestamp: Long)

// 2. The Stateful Process Function (Runs concurrently across hundreds of nodes)
class FraudDetector extends KeyedProcessFunction[String, TransactionEvent, FraudAlert] {

  // 3. Define the managed state (Automatically backed up to RocksDB and HDFS by Flink)
  @transient private var flagState: ValueState[java.lang.Boolean] = _
  @transient private var timerState: ValueState[java.lang.Long] = _

  override def open(parameters: Configuration): Unit = {
    // Initialize the state descriptors
    val flagDescriptor = new ValueStateDescriptor("small-transaction-flag", classOf[java.lang.Boolean])
    flagState = getRuntimeContext.getState(flagDescriptor)

    val timerDescriptor = new ValueStateDescriptor("timer-state", classOf[java.lang.Long])
    timerState = getRuntimeContext.getState(timerDescriptor)
  }

  // 4. The Core Evaluation Logic (Triggered for every single event)
  override def processElement(
      event: TransactionEvent,
      ctx: KeyedProcessFunction[String, TransactionEvent, FraudAlert]#Context,
      out: Collector[FraudAlert]): Unit = {

    // Retrieve the current state for this specific accountId (The Key)
    val lastTransactionWasSmall = flagState.value()

    if (lastTransactionWasSmall != null) {
      if (event.amount > 5000.00) {
        // PATTERN MATCHED: A massive transaction followed a small one!
        val alert = FraudAlert(event.accountId, "Massive spike after probing transaction", event.timestamp)
        out.collect(alert)
        cleanUp(ctx) // Clear the state to prevent memory leaks
      } else if (event.amount < 5.00) {
        // Still receiving small probing transactions, do nothing, let the timer keep running
      } else {
        // Normal transaction sequence, reset our state
        cleanUp(ctx)
      }
    }

    if (event.amount < 5.00 && flagState.value() == null) {
      // Set the flag for a small probing transaction
      flagState.update(true)

      // 5. Register a timer for 10 minutes in the future (Event Time, not Processing Time)
      val timer = ctx.timestamp() + (10 * 60 * 1000L)
      ctx.timerService().registerEventTimeTimer(timer)
      timerState.update(timer)
    }
  }

  // 6. The Async Timer Callback (Fires if the 10-minute window expires without fraud)
  override def onTimer(
      timestamp: Long,
      ctx: KeyedProcessFunction[String, TransactionEvent, FraudAlert]#OnTimerContext,
      out: Collector[FraudAlert]): Unit = {
    
    // The window has passed without a massive transaction. Clear the memory.
    timerState.clear()
    flagState.clear()
  }

  private def cleanUp(ctx: KeyedProcessFunction[String, TransactionEvent, FraudAlert]#Context): Unit = {
    val timer = timerState.value()
    if (timer != null) {
      ctx.timerService().deleteEventTimeTimer(timer)
    }
    timerState.clear()
    flagState.clear()
  }
}
