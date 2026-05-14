import akka.actor.typed.Behavior
import akka.actor.typed.scaladsl.Behaviors
import scala.collection.immutable.TreeMap

object OrderBookEngine {
  // 1. Domain Modeling (Strictly Immutable)
  sealed trait Command
  final case class SubmitOrder(orderId: String, isBuy: Boolean, price: Double, quantity: Int) extends Command
  final case class CancelOrder(orderId: String) extends Command
  
  final case class TradeExecution(makerOrderId: String, takerOrderId: String, price: Double, quantity: Int)

  final case class Order(id: String, price: Double, quantity: Int, timestamp: Long)

  // 2. The Core State (Bids sorted descending, Asks sorted ascending)
  final case class OrderBookState(
      bids: TreeMap[Double, Vector[Order]](Ordering[Double].reverse),
      asks: TreeMap[Double, Vector[Order]]
  )

  // 3. The Actor Behavior (Lock-free concurrency)
  def apply(): Behavior[Command] = running(OrderBookState(TreeMap.empty, TreeMap.empty))

  private def running(state: OrderBookState): Behavior[Command] = Behaviors.receive { (context, message) =>
    message match {
      case SubmitOrder(id, isBuy, price, quantity) =>
        val newOrder = Order(id, price, quantity, System.nanoTime())
        val (newState, executions) = if (isBuy) matchOrder(newOrder, state.asks, state.bids, isBuy = true)
                                     else matchOrder(newOrder, state.bids, state.asks, isBuy = false)

        executions.foreach { exec =>
          context.log.info(s"[HFT EXECUTION] Matched ${exec.quantity} @ $$${exec.price}. Maker: ${exec.makerOrderId}, Taker: ${exec.takerOrderId}")
        }

        running(if (isBuy) state.copy(asks = newState._1, bids = newState._2) 
                else state.copy(bids = newState._1, asks = newState._2))

      case CancelOrder(_) =>
        // Omitted for brevity: Deep tree traversal to remove order and return new state
        Behaviors.same
    }
  }

  // 4. The Matching Algorithm (Pure function, deeply recursive)
  private def matchOrder(
      taker: Order,
      makerBook: TreeMap[Double, Vector[Order]],
      takerBook: TreeMap[Double, Vector[Order]],
      isBuy: Boolean
  ): ((TreeMap[Double, Vector[Order]], TreeMap[Double, Vector[Order]]), List[TradeExecution]) = {
    
    // Check if the best price crosses the spread
    makerBook.headOption match {
      case Some((bestPrice, orders)) if (isBuy && taker.price >= bestPrice) || (!isBuy && taker.price <= bestPrice) =>
        val maker = orders.head
        val tradeQty = Math.min(taker.quantity, maker.quantity)
        val exec = TradeExecution(maker.id, taker.id, bestPrice, tradeQty)

        val updatedMakerOrders = if (maker.quantity == tradeQty) orders.tail else maker.copy(quantity = maker.quantity - tradeQty) +: orders.tail
        val updatedMakerBook = if (updatedMakerOrders.isEmpty) makerBook - bestPrice else makerBook.updated(bestPrice, updatedMakerOrders)

        if (taker.quantity > tradeQty) {
          // Taker still has quantity left, recursively match deeper into the book
          val (nextState, nextExecs) = matchOrder(taker.copy(quantity = taker.quantity - tradeQty), updatedMakerBook, takerBook, isBuy)
          (nextState, exec :: nextExecs)
        } else {
          // Taker is fully filled
          ((updatedMakerBook, takerBook), List(exec))
        }

      case _ =>
        // No match found, add to the taker's side of the book (resting order)
        val currentOrders = takerBook.getOrElse(taker.price, Vector.empty)
        ((makerBook, takerBook.updated(taker.price, currentOrders :+ taker)), Nil)
    }
  }
}
