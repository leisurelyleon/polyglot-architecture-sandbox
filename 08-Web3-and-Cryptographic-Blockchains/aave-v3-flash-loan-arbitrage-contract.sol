// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

import {FlashLoanSimpleReceiverBase} from "@aave/core-v3/contracts/flashloan/base/FlashLoanSimpleReceiverBase.sol";
import {IPoolAddressesProvider} from "@aave/core-v3/contracts/interfaces/IPoolAddressesProvider.sol";
import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {ISwapRouter} from "@uniswap/v3-periphery/contracts/interfaces/ISwapRouter.sol";

contract ArbitrageFlashbot is FlashLoanSimpleReceiverBase {
    ISwapRouter public immutable uniswapRouter;
    address public owner;

    error ArbitrageUnprofitable(uint256 expected, uint256 actual);
    error Unauthorized();

    modifier onlyOwner() {
        if (msg.sender != owner) revert Unauthorized();
        _;
    }

    constructor(address _addressProvider, address _uniswapRouter) 
        FlashLoanSimpleReceiverBase(IPoolAddressesProvider(_addressProvider)) 
    {
        uniswapRouter = ISwapRouter(_uniswapRouter);
        owner = msg.sender;
    }

    // 1. The Entry Point: Requesting the Flash Loan
    function executeArbitrage(address asset, uint256 amount) external onlyOwner {
        address receiverAddress = address(this);
        bytes memory params = ""; // Passed to the callback
        uint16 referralCode = 0;

        // Calls Aave to transfer 'amount' of 'asset' to this contract immediately
        POOL.flashLoanSimple(
            receiverAddress,
            asset,
            amount,
            params,
            referralCode
        );
    }

    // 2. The Callback: Aave hands over control to us (We have the millions of dollars now)
    function executeOperation(
        address asset,
        uint256 amount,
        uint256 premium,
        address initiator,
        bytes calldata params
    ) external override returns (bool) {
        
        // --- ARBITRAGE LOGIC ---
        // E.g., Swap borrowed DAI for WETH on Uniswap, then swap WETH back for DAI on Sushiswap
        
        uint256 amountToOwe = amount + premium;

        // Approve Uniswap router to spend our borrowed assets
        IERC20(asset).approve(address(uniswapRouter), amount);

        // Execute a Single-Hop Exact Input Swap on Uniswap V3
        ISwapRouter.ExactInputSingleParams memory swapParams = ISwapRouter.ExactInputSingleParams({
            tokenIn: asset,
            tokenOut: 0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2, // WETH
            fee: 3000, // 0.3% pool
            recipient: address(this),
            deadline: block.timestamp,
            amountIn: amount,
            amountOutMinimum: 0,
            sqrtPriceLimitX96: 0
        });

        uint256 wethReceived = uniswapRouter.exactInputSingle(swapParams);

        // *Imagine second swap from WETH back to DAI here*
        uint256 finalBalance = IERC20(asset).balanceOf(address(this));

        // If we didn't make enough to cover the loan + Aave premium, revert the entire transaction
        if (finalBalance < amountToOwe) {
            revert ArbitrageUnprofitable(amountToOwe, finalBalance);
        }

        // 3. Repayment: Approve Aave to pull the borrowed amount + fee back out
        IERC20(asset).approve(address(POOL), amountToOwe);

        return true;
    }

    // 4. Withdraw the profit to the bot owner
    function withdrawProfit(address asset) external onlyOwner {
        uint256 balance = IERC20(asset).balanceOf(address(this));
        IERC20(asset).transfer(owner, balance);
    }
}
