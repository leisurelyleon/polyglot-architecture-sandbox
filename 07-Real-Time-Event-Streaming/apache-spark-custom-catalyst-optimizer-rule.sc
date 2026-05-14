import org.apache.spark.sql.catalyst.expressions._
import org.apache.spark.sql.catalyst.plans.logical.LogicalPlan
import org.apache.spark.sql.catalyst.rules.Rule
import org.apache.spark.sql.types.{IntegerType, LongType, DoubleType}

// 1. Defining a custom compiler rule extending Spark's internal Optimizer
object SimplifyMultiplyByOne extends Rule[LogicalPlan] {
  
  // 2. The core logic applied to the Logical Plan (The Abstract Syntax Tree)
  def apply(plan: LogicalPlan): LogicalPlan = {
    // 'transformAllExpressions' recursively visits every node in the query plan
    plan.transformAllExpressions {
      
      // 3. Deep Pattern Matching against the AST nodes
      case multiply @ Multiply(left, right, failOnError) =>
        
        // If the right side is a literal '1', return just the left side
        if (isOne(right)) {
          left
        } 
        // If the left side is a literal '1', return just the right side
        else if (isOne(left)) {
          right
        } 
        // Otherwise, leave the multiplication node exactly as it is
        else {
          multiply
        }
    }
  }

  // 4. Helper function to safely check literal types across Spark's memory representations
  private def isOne(expression: Expression): Boolean = expression match {
    case Literal(value, IntegerType) => value.asInstanceOf[Int] == 1
    case Literal(value, LongType)    => value.asInstanceOf[Long] == 1L
    case Literal(value, DoubleType)  => value.asInstanceOf[Double] == 1.0
    case _                           => false
  }
}

/* 
 * Usage in a Spark Session:
 * spark.experimental.extraOptimizations = Seq(SimplifyMultiplyByOne)
 * spark.sql("SELECT salary * 1 FROM employees").explain(true) 
 * // The physical plan will completely omit the multiplication step!
 */
