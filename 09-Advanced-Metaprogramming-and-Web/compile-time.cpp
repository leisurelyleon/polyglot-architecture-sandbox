#include <iostream>

// Helper template to check if a number has any divisors up to a limit
template <int N, int D>
struct IsPrimeHelper {
    static constexpr bool value = (N % D != 0) && IsPrimeHelper<N, D - 1>::value;
};

// Base case for the helper: when divisor reaches 1, no smaller divisors exist
template <int N>
struct IsPrimeHelper<N, 1> {
    static constexpr bool value = true;
};

// Main template to evalutate primality
template <int N>
struct IsPrime {
    static constexpr bool value = IsPrimeHelper<N, N / 2>::value;
};

// Edge cases for 0, 1, and 2
template <> struct IsPrime<0> { static constexpr bool value = false; };
template <> struct IsPrime<1> { static constexpr bool value = false; };
template <> struct IsPrime<2> { static constexpr bool value = true; };

// A structure to hold our generated list of primes
template <int... Primes>
struct PrimeList {};

// Recursive struct to generate primes up to Max
template <int Current, int Max, bool CurrentIsPrime, int... FoundPrimes>
struct PrimeGenerator;

// Specialization for when the Current number IS prime
template <int Current, int Max, int... FoundPrimes>
struct PrimeGenerator<Current, Max, true, FoundPrimes...> {
    using type = typename PrimeGenerator<Current + 1, Max, IsPrime<Current + 1>::value, FoundPrimes..., Current>::type;
};

// Specialization for when the Current number IS NOT prime
template <int Current, int Max, int... FoundPrimes>
struct PrimeGenerator<Current, Max, false, FoundPrimes...> {
    using type = typename PrimeGenerator<Current + 1, Max, IsPrime<Current + 1>::value, FoundPrimes...>::type;
};

// Base case: we've exceeded Max, return the assembled PrimeList
template <int Max, bool AnyPrime, int... FoundPrimes>
struct PrimeGenerator<Max, Max, AnyPrime, FoundPrimes...> {
    using type = PrimeList<FoundPrimes...>;
};

// Utility function to print our compile-time list using fold expressions
template <int... Primes>
void printPrimes(PrimeList<Primes...>) {
    ((std::cout << Primes << " "), ...);
    std::cout << std::endl;
}

int main() {
    // Generate a list of prime numbers up to 50 AT A COMPILE TIME
    using PrimesUpTo50 = PrimeGenerator<2, 50, IsPrime<2>::value>::type;

    std::cout << "Compile-time generated prime numbers up to 50: \n";
    printPrimes(PrimesUpTo50{});

    return 0;
}
