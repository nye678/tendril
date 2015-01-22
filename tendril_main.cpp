#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <stack>
#include <queue>
#include <vector>
#include <unordered_map>

const char* prompt = "[]> ";
const char* quit = "quit";
const char* whitespace = " ";

struct cell;

std::unordered_map<std::string, cell> env;

#define TOKEN_LPAREN 1
#define TOKEN_RPAREN 2
#define TOKEN_TOKEN 3

struct token
{
	char* str;
	size_t len;
	uint32_t type;
};

enum cell_type { CELL_SYMBOL, CELL_NUMBER, CELL_LIST, CELL_PROC, CELL_LAMBDA, CELL_STRING };

struct cell
{
	typedef cell (*cell_proc)(std::vector<cell> &);

	std::vector<cell> list;
	cell_type type;

	std::string str;
	cell_proc proc;
	int32_t i;
	float f;
};

cell create_symbol_cell(char* str)
{
	cell c;
	c.type = CELL_SYMBOL;
	c.str = std::string(str);
	return c;
}

cell create_proc_cell(cell::cell_proc proc)
{
	cell c;
	c.type = CELL_PROC;
	c.proc = proc;
	return c;
}

cell create_number_cell(int32_t i)
{
	cell c;
	c.type = CELL_NUMBER;
	c.i = i;
	return c;
}

cell create_string_cell(char* str)
{
	cell c;
	c.type = CELL_STRING;
	c.str = std::string(str);
	return c;
}

cell create_list_cell(std::vector<cell> &cells)
{
	cell c;
	c.type = CELL_LIST;
	c.list = cells;
	return c;
}

const cell nil = create_symbol_cell("nil");
const cell trueCell = create_symbol_cell("T");

bool isNumber(char* str, size_t len)
{
	if (len == 0)
		return false;

	if (len == 1)
		return (*str >= '0' && *str <= '9');

	bool result = *str == '-' || (*str >= '0' && *str <= '9');
	for (size_t i = 1; i < len; ++i)
	{
		result = result & (str[i] >= '0' && str[i] <= '9');
	}

	return result;
}

bool isString(char* str, size_t len)
{
	if (len < 2)
		return false;
	return str[0] == '\"' && str[len - 1] == '\"';
}

cell parse_cell(std::queue<token> &tokens)
{
	token tok = tokens.front();
	tokens.pop();
	if (tok.type == TOKEN_LPAREN)
	{
		cell c;
		c.type = CELL_LIST;
		while (!tokens.empty() && tokens.front().type != TOKEN_RPAREN)
		{
			c.list.push_back(parse_cell(tokens));
		}
		tokens.pop();
		return c;
	}
	else
	{
		cell c;
		
		char* buff = (char*)calloc(tok.len + 1, 1);
		strncpy(buff, tok.str, tok.len);
		
		if (isNumber(tok.str, tok.len))
		{
			c = create_number_cell(atoi(buff));
		}
		else if (isString(tok.str, tok.len))
		{
			c = create_string_cell(buff);
		}
		else
		{
			c = create_symbol_cell(buff);
		}

		free(buff);
		return c;
	}
}

void print_cell(cell &c, size_t level = 0)
{
	for (size_t i = 0; i < level; ++i)
	{
		printf("\t");
	}

	if (c.type == CELL_SYMBOL)
	{
		printf("CELL_SYMBOL(%s)\n", c.str);
	}
	else if (c.type == CELL_NUMBER)
	{
		printf("CELL_NUMBER(%d)\n", c.i);
	}
	else if (c.type == CELL_LIST)
	{
		printf("CELL_LIST\n");
		for (auto cc : c.list)
		{
			print_cell(cc, level + 1);
		}
	}
	else if (c.type == CELL_PROC)
	{
		printf("CELL_PROC\n");
	}
	else if (c.type == CELL_LAMBDA)
	{
		printf("CELL_LAMBDA\n");
	}
	else if (c.type == CELL_STRING)
	{
		printf("CELL_STRING(\"%s\")\n", c.str);
	}
}

cell proc_plus(std::vector<cell> &cells)
{
	cell sum = create_number_cell(0);
	for (auto c : cells)
	{
		sum.i += c.i;
	}

	return sum;
}

cell proc_minus(std::vector<cell> &cells)
{
	cell diff = create_number_cell(cells.begin()->i);
	for (auto c = cells.begin() + 1; c != cells.end(); ++c)
	{
		diff.i -= c->i;
	}

	return diff;
}

cell proc_multiply(std::vector<cell> &cells)
{
	cell prod = create_number_cell(1);
	for (auto c : cells)
	{
		prod.i *= c.i;
	}

	return prod;
}

cell proc_divide(std::vector<cell> &cells)
{
	cell quot = create_number_cell(cells.begin()->i);
	for (auto c = cells.begin() + 1; c != cells.end(); ++c)
	{
		quot.i /= c->i;
	}

	return quot;
}

cell proc_equals(std::vector<cell> &cells)
{
	auto c1 = cells.begin();
	auto c2 = c1 + 1;
	if (c1->type != c2->type)
	{
		return nil;
	}

	cell_type type = c1->type;
	switch (type)
	{
		case CELL_SYMBOL:
		case CELL_STRING:
		{
			return (c1->str.compare(c2->str)) == 0 ? trueCell : nil;
		}
		break;
		case CELL_NUMBER:
		{
			return (c1->i == c2->i) ? trueCell : nil;
		}
		break;
		case CELL_PROC:
		{
			return (c1->proc == c2->proc) ? trueCell : nil;
		}
		break;
		default:
			return nil;
	}
}

cell proc_atom(std::vector<cell> &cells)
{
	return (cells.begin()->type != CELL_LIST) ? trueCell : nil;
}

cell proc_list(std::vector<cell> &cells)
{
	return create_list_cell(cells);
}

cell eval(cell &c)
{
	if (c.type == CELL_SYMBOL)
		return env[c.str];

	if (c.type == CELL_NUMBER || c.type == CELL_STRING)
		return c;

	cell proc(eval(c.list[0]));
	std::vector<cell> exps;
	for (auto itor = c.list.begin() + 1; itor != c.list.end(); ++itor)
	{
		exps.push_back(eval(*itor));
	}
	
	if (proc.type == CELL_PROC)
	{
		return proc.proc(exps);
	}

	return nil;
}

int main(int argc, char* argv[])
{
	bool running = true;
	char* buffer = (char*)malloc(2048);

	env["nil"] = nil;
	env["T"] = trueCell;
	env["+"] = create_proc_cell(proc_plus);
	env["-"] = create_proc_cell(proc_minus);
	env["*"] = create_proc_cell(proc_multiply);
	env["/"] = create_proc_cell(proc_divide);
	env["="] = create_proc_cell(proc_equals);
	env["eq"] = create_proc_cell(proc_equals);
	env["atom"] = create_proc_cell(proc_atom);
	env["list"] = create_proc_cell(proc_list);

	while(running)
	{
		printf(prompt);
		gets(buffer);

		if (strcmp(buffer, quit) == 0)
		{
			running = false;
		}
		else
		{
			// Tokenize the string
			std::queue<token> tokens;
			size_t len = strlen(buffer);
			char* s = buffer;
			while (*s)
			{
				while (*s == ' ')
				{
					++s;
				}
				if (*s == '(' || *s == ')')
				{
					token tok = { s, 1, ((*s == '(') ? TOKEN_LPAREN : TOKEN_RPAREN) };
					tokens.push(tok);
					++s;
				}
				else
				{
					char* t = s;
					while (*t && *t != ' ' && *t != '(' && *t != ')')
					{
						++t;
					}

					token tok = { s, t - s, TOKEN_TOKEN };
					tokens.push(tok);
					s = t;
				}
			}

			cell c = parse_cell(tokens);
			//print_cell(c);
			cell e = eval(c);
			print_cell(e);


			// Prints all the tokens
			/*for (token tok; !tokens.empty(); tokens.pop())
			{
				tok = tokens.front();
				if (tok.type == TOKEN_LPAREN)
					printf("TOKEN_LPAREN\n");
				else if (tok.type == TOKEN_RPAREN)
					printf("TOKEN_RPAREN\n");
				else if (tok.type == TOKEN_TOKEN)
				{
					char buff[256] = {};
					strncpy(buff, tok.str, tok.len);

					printf("TOKEN_TOKEN(\"%s\")\n", buff);
				}
			}*/
		}
	}

	free(buffer);

	return 0;
}