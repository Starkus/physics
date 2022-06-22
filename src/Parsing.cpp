struct Tokenizer
{
	const char *cursor;
	const char *end;
	s64 line;
};

enum TokenType
{
	TOKEN_INVALID,
	TOKEN_IDENTIFIER,
	TOKEN_LITERAL_NUMBER,
	TOKEN_LITERAL_STRING,
	TOKEN_LITERAL_CHAR,
	TOKEN_PREPROCESSOR_DIRECTIVE,
	TOKEN_ASCII_BEGIN = '!',
	TOKEN_ASCII_END = '~' + 1,
	TOKEN_END_OF_FILE
};

struct Token
{
	s64 type;
	const char *begin;
	s64 size;

	const char *file;
	s64 line;
};
DECLARE_DYNAMIC_ARRAY(Token);

int EatWhitespace(Tokenizer *tokenizer)
{
	int nAdvanced = 0;
	while (*tokenizer->cursor && IsWhitespace(*tokenizer->cursor))
	{
		if (*tokenizer->cursor == '\n')
			++tokenizer->line;
		++tokenizer->cursor;
		++nAdvanced;
	}
	return nAdvanced;
}

int EatRestOfLine(Tokenizer *tokenizer)
{
	int nAdvanced = 0;
	for (;; ++tokenizer->cursor)
	{
		char c = *tokenizer->cursor;
		if (!c)
			break;

		if (c == '\\')
		{
			++tokenizer->cursor;
		}
		else if (c == '\n')
		{
			++tokenizer->cursor;
			++tokenizer->line;
			break;
		}
		++nAdvanced;
	}
	return nAdvanced;
}

void ProcessCppComment(Tokenizer *tokenizer)
{
	EatRestOfLine(tokenizer);
}

void ProcessCComment(Tokenizer *tokenizer)
{
	tokenizer->cursor += 2;
	for (;; ++tokenizer->cursor)
	{
		if (!*tokenizer->cursor)
			break;

		if (*tokenizer->cursor == '*' && *(tokenizer->cursor + 1) == '/')
		{
			tokenizer->cursor += 2;
			break;
		}
	}
}

Token ReadTokenAndAdvance(Tokenizer *tokenizer)
{
	Token result = {};
	result.line = tokenizer->line;

	EatWhitespace(tokenizer);

	while (*tokenizer->cursor == '/')
	{
		if (*(tokenizer->cursor + 1) == '/')
		{
			ProcessCppComment(tokenizer);
			EatWhitespace(tokenizer);
		}
		else if (*(tokenizer->cursor + 1) == '*')
		{
			ProcessCComment(tokenizer);
			EatWhitespace(tokenizer);
		}
		else
			break;
	}

	result.begin = tokenizer->cursor;

	if (!*tokenizer->cursor || tokenizer->cursor >= tokenizer->end)
	{
		result.type = TOKEN_END_OF_FILE;
		return result;
	}

	if (*tokenizer->cursor == '"')
	{
		result.type = TOKEN_LITERAL_STRING;
		++tokenizer->cursor;
		result.begin = tokenizer->cursor;

		while (*tokenizer->cursor && *tokenizer->cursor != '"')
		{
			if (*tokenizer->cursor == '\\')
			{
				++result.size;
				++tokenizer->cursor;
			}
			++result.size;
			++tokenizer->cursor;
		}
		++tokenizer->cursor;
		//Log("String literal: \"%.*s\"\n", result.size, result.begin);
	}
	else if (*tokenizer->cursor == '\'')
	{
		result.type = TOKEN_LITERAL_CHAR;
		++tokenizer->cursor;
		if (*tokenizer->cursor == '\\')
		{
			++tokenizer->cursor;
		}
		++tokenizer->cursor;
		ASSERT(*tokenizer->cursor == '\'');
		++tokenizer->cursor;
	}
	else if (IsAlpha(*tokenizer->cursor) || *tokenizer->cursor == '_')
	{
		result.type = TOKEN_IDENTIFIER;
		while (IsAlpha(*tokenizer->cursor) || IsNumeric(*tokenizer->cursor) || *tokenizer->cursor == '_')
		{
			++result.size;
			++tokenizer->cursor;
		}
		//Log("Identifier: %.*s\n", result.size, result.begin);
	}
	else if (IsNumeric(*tokenizer->cursor))
	{
		result.type = TOKEN_LITERAL_NUMBER;
		bool done = false;
		if (*tokenizer->cursor == '0')
		{
			done = true;
			++result.size;
			++tokenizer->cursor;
			ASSERT(!IsNumeric(*tokenizer->cursor));
			if (*tokenizer->cursor == 'x' ||
					*tokenizer->cursor == 'X' ||
					*tokenizer->cursor == 'b' ||
					*tokenizer->cursor == '.')
			{
				done = false;
				++result.size;
				++tokenizer->cursor;
			}
		}
		if (!done)
		{
			while (IsNumericHex(*tokenizer->cursor) || *tokenizer->cursor == '.')
			{
				++result.size;
				++tokenizer->cursor;
			}
			if (*tokenizer->cursor == 'f')
			{
				++result.size;
				++tokenizer->cursor;
			}
		}
		//Log("Number: %.*s\n", result.size, result.begin);
	}
	else if (*tokenizer->cursor == '#')
	{
		result.type = TOKEN_PREPROCESSOR_DIRECTIVE;
		result.size = EatRestOfLine(tokenizer);
		//Log("Preprocessor directive: %.*s\n", result.size, result.begin);
	}
	else if (*tokenizer->cursor >= TOKEN_ASCII_BEGIN && *tokenizer->cursor < TOKEN_ASCII_END)
	{
		result.type = *tokenizer->cursor;
		++tokenizer->cursor;
	}
	else
	{
		result.type = TOKEN_INVALID;
		result.begin = tokenizer->cursor;
		result.size = 1;
		++tokenizer->cursor;
	}

	return result;
}

inline bool TokenIsStr(Token *token, const char *str)
{
	return token->type == TOKEN_IDENTIFIER &&
		token->size == (s64)strlen(str) &&
		strncmp(token->begin, str, token->size) == 0;
}

inline bool TokenIsEqual(Token *a, Token *b)
{
	if (a->type != b->type)
		return false;
	if (a->type == TOKEN_IDENTIFIER)
		return a->size == b->size && strncmp(a->begin, b->begin, a->size) == 0;
	return true;
}

void TokenizeFile(const char *fileBuffer, u64 fileSize, DynamicArray_Token &tokens,
		const char *filename = nullptr)
{
	// @Fix: allocating functions in this procedure! We don't really know how to expand the tokens
	// array.

	const char *currentFile = filename;
	s64 currentLineOffset = 0;

	Tokenizer tokenizer = {};
	tokenizer.cursor = fileBuffer;
	tokenizer.end = fileBuffer + fileSize;
	tokenizer.line = 1; // Line numbers start at 1 not 0.
	while (true)
	{
		Token newToken = ReadTokenAndAdvance(&tokenizer);

		if (newToken.type == TOKEN_PREPROCESSOR_DIRECTIVE)
		{
			if (strncmp(newToken.begin, "#line ", Min(6, newToken.size)) == 0)
			{
				const char *lineNumberStart = newToken.begin + 6;
				int line = atoi(lineNumberStart);

				currentLineOffset = line - newToken.line - 1; // Not 100% sure why the -1 but w/e.

				const char *filenameStart = lineNumberStart;
				while (*filenameStart != '"')
					++filenameStart;
				++filenameStart;

				const char *filenameEnd = filenameStart;
				while (*filenameEnd != '"')
					++filenameEnd;

				u64 filenameSize = filenameEnd - filenameStart;
				char *newStr = (char *)malloc(filenameSize + 1); // @Fix: hardcoded allocator.
				strncpy(newStr, filenameStart, filenameSize);
				newStr[filenameSize] = 0;
				currentFile = newStr;
			}
		}
		else
		{
			newToken.file = currentFile;
			newToken.line += currentLineOffset;
			*DynamicArrayAdd_Token(&tokens, realloc) = newToken;
		}

		if (newToken.type == TOKEN_END_OF_FILE)
			break;
	}
}

