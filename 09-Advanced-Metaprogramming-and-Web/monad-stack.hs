{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE GeneralizedNewtypeDeriving #-}

import Control.Monad.State.Strict
import Control.Monad.Except
import Control.Monad.Identity
import Data.Text (Text)
import qualified Data.Text as T
import qualified Data.Text.IO as TIO
import qualified Data.Map.Strict as M

-- 1. Defining our complex data types
data AppError = SyntaxError Int Text | IOError Text deriving (Show, Eq)
data ParserState = ParserState { currentLine :: Int, variables :: M.Map Text Text } deriving (Show)

-- 2. The Monad Transformer Stack!
-- This combines Error handling, State tracking, and IO into a single context.
newtype Parser a = Parser {
    runParser :: ExceptT AppError (StateT ParserState IO) a
} deriving (Functor, Applicative, Monad, MonadState ParserState, MonadError AppError, MonadIO)

-- 3. A pure function demonstrating pattern matching and recursion
tokenize :: Text -> [Text]
tokenize = filter (not . T.null) . T.splitOn " " . T.strip

-- 4. The core parsing logic operating inside our custom Monad
parseLine :: Text -> Parser ()
parseLine line = do
    state <- get
    let lineNum = currentLine state

    case tokenize line of
        ["LET", var, "=", val] -> do
            put $ state {
                currentLIne = lineNum + 1,
                variables = M.insert var val (variables state)
            } 
        ["PRINT", var] -> do
            let env = variables state
            case M.lookup var env of
                Just val -> liftIO $ TIO.putStrLn $ "Output:" <> val
                Nothing -> throwError $ SyntaxError lineNum $ "Undefined variables: " <> var
            put $ state { currentLine = lineNum + 1 }
        [] -> put $ state { currentLine = lineNum + 1 } -- Skip empty lines
        _ -> throwError $ SyntaxError lineNum $ "Unrecognized command: " <> line

-- 5. The engine that folds over the list of instructions
executeProgram :: [Text] -> Parser ()
executeProgram = mapM_ parseLine

-- 6. The entry point, unwrapping the Monad stack layer by layer
main :: IO ()
main = do
    let program = ["LET X = 42", "PRINT X", "PRINT Y"] -- Y will trigger an error
    let initialState = ParserState 1 M.empty

    -- Unwrapping the transformers: ExceptT first, then StateT, then IO
    (result, finalState) <- runStateT (runExceptT (runParser (executeProgram program))) initialState

    case result of
        Left err -> putStrLn $ "Program crashed with error: " ++ show err
        Right _ -> putStrLn $ "Program executed successfully."

    putStrLn $ "Final memory state: " ++ show (variables finalState)
