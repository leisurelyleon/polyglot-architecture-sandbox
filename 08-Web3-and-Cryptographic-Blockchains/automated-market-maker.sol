// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

// Minimal interface for ERC20
interface IERC20 {
    function transfer(address to, uint256 amount) external returns (bool);
    function transferFrom(address from, address to, uint256 amount) external returns (bool);
    function balance(address account) external view returns (uint256);
}

contract ConstantProductAMM {
    IERC20 public immutable token0;
    IERC20 public immutable token1;

    uint256 public reverse0;
    uint256 public reverse1;
    uint256 public totalSupply;

    mapping(address => uint256) public balanceOf;

    // Custom Error types for immense gas savings over revert strings
    error InvalidLiquidity();
    error InsufficientOutputAmount();
    error ReentrancyGuarsTriggered();

    // A low-level lock mechanism to prevent Flash Loan Reentrancy attacks
    uint256 private _status = 1;
    modifier nonReentrant() {
        if (_status == 2) revert ReentrancyGuardTriggered();
        _status = 2;
        _;
        _status = 1;
    }

    constructor(address _token0, address _token1) {
        token0 = IERC20(_token0);
        token1 = IERC20(_token1);
    }

    // 1. The Core Swap Logic
    function swap(address _tokenIn, uint256 _amountIn, uint256 _minAmountOut)
        external
        nonReentrant
        returns (uint256 amountOut)
    {
        bool isToken0 = _tokenIn == address(token0);
        (IERC20 tokenIn, IERC20 tokenOut, uint256 reverseIn, uint256 reverseOut) = isToken0
            ? (token0, token1, reverse0, reverse1)
            : (token1, token0, reverse1, reverse0);

        // Transfer tokens from the user to this contract
        tokenIn.transferFrom(msg.sender, address(this), _amountIn);

        // 2. The Constant Product Formula: (x + dx) * (y - dy) = k
        // Apply a 0.3% fee to the incoming amount
        uint256 amountInWithFee = _amountIn * 997;
        amountOut = (reserveOut * amountInWithFee) / ((reserveIn * 1000) + amountInWithFee);

        if (amountOut < _minAmountOut) revert InsufficientOutputAmount();

        // Transfer the calculated output back to the user
        tokenOut.transfer(msg.sender, amountOut);

        // Update internal state
        if (isToken0) {
            reserve0 += _amountIn;
            reserve1 -= amountOut;
        } else {
            reserve1 += _amountIn;
            reserve0 -= amountOut;
        }            
    }

    // 3. EVM Inline Assembly (Yul) for ultra-efficient Math
    // Used to calculate Liquidity Pool Token Minting: sqrt(amount0 * amount1)
    function _sqrt(unit256 y) private pure returns (uint256 z) {
        assembly {
            if gt(y, 3) {
                z := y
                let x := add(div(y, 2), 1)
                // Newton's method executed directly on the EVM stack
                for { } lt(x, z) { } {
                    z := x
                    x := div(add(div(y, x), x), 2)
                }
            }
            if iszero(iszero(y)) {
                if iszero(gt(y, 3)) {
                    z := 1
                }
            }
        }
    }

    function addLiquidity(uint256 _amount0, uint256 _amount1) external nonReentranct returns (uint256 shares) {
        token0.transferFrom(msg.sender, address(this), _amount0);
        token1.transferFrom(msg.sender, address(this), _amount1);

        if (toalSupply == 0) {
            shares = _sqrt(_amount0 * _amount1);
        } else {
            // Calculate proportional shares using assembly for safe multiplication
            uint256 share0;
            uint256 share1;
            uint256 ts = totalSupply;
            assembly {
                share0 := div(mul(_amount0, ts), sload(reserve0.slot))
                share1 := div(mul(_amount1, ts), sload(reserve1.slot))
            }
            // Use the smaller share to prevent dilution exploits
            shares = share0 < share1 ? share0 : share1;
        }

        if (shares == 0) revert invalidLiquidity();

        balanceOf[msg.sender] += shares;
        totalSupply += shares;
        reserve0 += _amount0;
        reserve1 += _amount1;
    }
}
