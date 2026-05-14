using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Reflection;

namespace EnterpriseBackend.RulesEngine
{
    public class UserRecord
    {
        public Guid Id { get; set; }
        public string Username { get; set; }
        public int ReputationScore { get; set; }
        public bool IsActive { get; set; }
    }

    public static class DynamicQueryBuilder
    {
        /// <summary>
        /// Dynamically builds a Lambda Expression Func<T, bool> for IQueryable filtering.
        /// E.g., translates "ReputationScore", ">", "500" into u => u.ReputationScore > 500
        /// </summary>
        public static Expression<Func<T, bool>> BuildPredicate<T>(string propertyName, string comparisonOperator, object value)
        {
            // 1. Create the input parameter for the Lambda (e.g., 'x' in x => ...)
            ParameterExpression parameter = Expression.Parameter(typeof(T), "x");

            // 2. Use Reflection to find the property on the object
            PropertyInfo property = typeof(T).GetProperty(propertyName, BindingFlags.IgnoreCase | BindingFlags.Public | BindingFlags.Instance);
            if (property == null)
                throw new ArgumentException($"Property '{propertyName}' not found on type '{typeof(T).Name}'");

            // 3. Access the property on the parameter (e.g., x.ReputationScore)
            MemberExpression propertyAccess = Expression.MakeMemberAccess(parameter, property);

            // 4. Safely convert the incoming value object to the actual property type
            ConstantExpression constantValue = Expression.Constant(Convert.ChangeType(value, property.PropertyType));

            // 5. Build the binary expression based on the requested operator
            Expression binaryExpression;
            switch (comparisonOperator.ToLower())
            {
                case "eq":
                case "==":
                    binaryExpression = Expression.Equal(propertyAccess, constantValue);
                    break;
                case "gt":
                case ">":
                    binaryExpression = Expression.GreaterThan(propertyAccess, constantValue);
                    break;
                case "lt":
                case "<":
                    binaryExpression = Expression.LessThan(propertyAccess, constantValue);
                    break;
                case "contains":
                    // String specific method call generation: x.Username.Contains("value")
                    MethodInfo containsMethod = typeof(string).GetMethod("Contains", new[] { typeof(string) });
                    binaryExpression = Expression.Call(propertyAccess, containsMethod, constantValue);
                    break;
                default:
                    throw new NotSupportedException($"Operator '{comparisonOperator}' is not supported.");
            }

            // 6. Compile the Expression Tree into an executable Lambda
            return Expression.Lambda<Func<T, bool>>(binaryExpression, parameter);
        }
    }

    class Program
    {
        static void Main()
        {
            var dbMock = new List<UserRecord>
            {
                new UserRecord { Id = Guid.NewGuid(), Username = "Admin", ReputationScore = 9000, IsActive = true },
                new UserRecord { Id = Guid.NewGuid(), Username = "Guest", ReputationScore = 10, IsActive = false }
            }.AsQueryable();

            // At runtime, we receive strings from an API payload
            string apiFilterProp = "ReputationScore";
            string apiFilterOp = ">";
            int apiFilterValue = 1000;

            // Generate the dynamic lambda predicate
            var dynamicFilter = DynamicQueryBuilder.BuildPredicate<UserRecord>(apiFilterProp, apiFilterOp, apiFilterValue);

            // Apply it to our IQueryable (In EF Core, this gets translated directly to SQL)
            var highRepUsers = dbMock.Where(dynamicFilter).ToList();

            foreach(var user in highRepUsers)
            {
                Console.WriteLine($"Found High Rep User: {user.Username}");
            }
        }
    }
}
