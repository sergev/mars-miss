/*
 * формат       = задание [ [ , ] задание ...]
 * задание      = [ кратность ] описание
 * описание     = ( формат )
 *              | тип [ ширина [ . точность ] ]
 *              | ' текст '
 *              | P
 * тип          = I | F | E | D | A | X | /
 * кратность    = целое
 * ширина       = целое
 * точность     = целое
 *
 * I5           - целое поле
 * F12.4        - вещественное поле
 * E12.4        - вещественное поле
 * D12.4        - вещественное поле
 * A48          - текстовое поле
 * X            - пропуск
 * 3Hfoo        - текст
 * 'foo'        - текст
 * /            - новая строка
 * nP           - масштабный коэффициент
 */

#include <string.h>
#include "format.h"

#define MAXSTACK 8                              /* Макс. вложенность скобок */

struct stack {                                  /* Стек скобок */
	char *ptr;
	int type;
	int count;
	int width;
	int prec;
	char *text;
};

int FormatWidth;                                /* Текущая ширина поля */
int FormatPrecision;                            /* Текущая точность */
int FormatScale;                                /* Масштабный коэффициент */
char *FormatText;                               /* Текстовое поле */

static char *Format;                            /* Указатель на формат */
static char *FormatEnd;                         /* Конец формата */
static struct stack FormatStack [MAXSTACK];     /* Стек вложенных повторов */
static int FormatLevel;                         /* Текущий уровень стека */
static char *FirstBrace;                        /* Внешняя левая скобка */
static int FirstBraceCount;                     /* И ее кратность */
static int FirstFormatScale;                    /* Масштаб на первой скобке */

extern long strtol (char *string, char **endptr, int base);

/*
 * Задание формата для разбора.
 */
void FormatInit (char *f)
{
	Format = f;
	if (*Format == '(') {
		int level;

		++Format;
		for (level=0; *f && level>=0; ++f) {
			if (*f == '(')
				++level;
			else if (*f == ')')
				--level;
		}
		FormatEnd = f;
	} else
		FormatEnd = Format + strlen (Format);
	FormatLevel = 0;
	FormatStack[0].ptr = Format;
	FormatStack[0].count = 0;
	FirstBrace = 0;
	FormatScale = FirstFormatScale = 0;
}

/*
 * Запрос следующего поля формата.
 * Возвращает тип поля: 'I', 'F', 'E', 'D', 'A', 'X', 'H', '/', '*'.
 * Ширина и точность поля, а также указатель на текстовое поле
 * возвращаются в переменных FormatWidth, FormatPrecision, FormatText.
 * Коэффициент масштабирования - в переменной FormatScale.
 */
int FormatNext ()
{
	struct stack *stack = &FormatStack[FormatLevel];
	char *p;
	long count;

	/* Повторяем предыдущий формат */
	if (stack->count > 0) {
		--stack->count;
		FormatWidth = stack->width;
		FormatPrecision = stack->prec;
		FormatText = stack->text;
		return (stack->type);
	}
	p = stack->ptr;

	/* Ищем новый формат */
	count = 1;
	FormatWidth = FormatPrecision = 0;
	for (; p<FormatEnd; ++p) {
		switch (*p) {
		default:
			/* Error: invalid format character *p */
		case ',':
nextfield:              count = 1;
			FormatWidth = FormatPrecision = 0;
			continue;
		case ' ':
			continue;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			count = strtol (p, &p, 10);
			--p;
			continue;
		case 'P': case 'p':
			FormatScale = count;
			count = 1;
			continue;
		case '/':
			stack->type = *p++;
			goto doformat;
		case 'x': stack->type = 'X'; ++p; goto dowidth;
		case 'i': stack->type = 'I'; ++p; goto dowidth;
		case 'f': stack->type = 'F'; ++p; goto dowidth;
		case 'e': stack->type = 'E'; ++p; goto dowidth;
		case 'd': stack->type = 'D'; ++p; goto dowidth;
		case 'a': stack->type = 'A'; ++p; goto dowidth;
		case 'I': case 'F': case 'E': case 'D': case 'A': case 'X':
			stack->type = *p++;
dowidth:
			if (count == 0)
				count = 1;
			if (*p>='0' && *p<='9')
				FormatWidth = strtol (p, &p, 10);
			if (*p == '.')
				FormatPrecision = strtol (p+1, &p, 10);
			if (stack->type == 'X') {
				if (FormatWidth)
					FormatWidth *= count;
				else
					FormatWidth = count;
				count = 1;
			}
doformat:
			stack->count = count - 1;
			stack->width = FormatWidth;
			stack->prec = FormatPrecision;
			stack->ptr = p;
			return (stack->type);
		case 'H': case 'h':
			FormatText = ++p;
			FormatWidth = count;
			count = 1;
			if (strlen (FormatText) < FormatWidth) {
				/* Error: too long 'H' format */
				FormatWidth = strlen (FormatText);
			}
			p = FormatText + FormatWidth;
dotext:
			stack->type = 'H';
			stack->count = count - 1;
			stack->width = FormatWidth;
			stack->prec = FormatPrecision = 0;
			stack->text = FormatText;
			stack->ptr = p;
			return ('H');
		case '\'':
		case '"':
			FormatText = ++p;
			p = strchr (p, p[-1]);
			if (p) {
				FormatWidth = p - FormatText;
				++p;
			}
			else {
				/* Error: unterminated character constant */
				FormatWidth = strlen (FormatText);
				p = FormatText + FormatWidth;
			}
			goto dotext;
		case '(':
			if (FormatLevel == MAXSTACK-1) {
				/* Error: too deep '(' */
				goto nextfield;
			}
			stack->ptr = p+1;
			stack->count = count - 1;
			if (! FirstBrace) {
				FirstBrace = stack->ptr;
				FirstBraceCount = stack->count + 1;
				FirstFormatScale = FormatScale;
			}
			++FormatLevel;
			++stack;
			goto nextfield;
		case ')':
			if (FormatLevel == 0) {
				/* Error: unbalanced ')' */
				goto nextfield;
			}
			if (stack[-1].count > 0) {
				--stack[-1].count;
				p = stack[-1].ptr - 1;
				goto nextfield;
			}
			--FormatLevel;
			--stack;
			continue;
		}
	}
	if (FormatLevel != 0) {
		/* Error: unbalanced '(' */
		FormatLevel = 0;
	}
	FormatScale = FirstFormatScale;
	if (FirstBrace) {
		FormatStack[0].ptr = FirstBrace;
		FormatStack[0].count = FirstBraceCount;
		FormatLevel = 1;
	} else {
		FormatStack[0].ptr = Format;
		FormatStack[0].count = 0;
	}
	return ('*');
}

#ifdef DEBUGFORMAT
main ()
{
	int n, t;

	FormatInit ("8, 3x3, 2i7, () 2(2f9.5, 2(4habra)), 2('foo')");
	for (n=0; n<40; ++n) {
		t = FormatNext ();
		switch (t) {
		default: printf (" ?"); break;
		case '/': printf (" /\n"); break;
		case '*': printf (" *\n"); break;
		case 'X': printf (" %dx", FormatWidth); break;
		case 'H': printf (" '%.*s'", FormatWidth, FormatText); break;
		case 'I': printf (" i%d", FormatWidth); break;
		case 'A': printf (" a%d", FormatWidth); break;
		case 'F': printf (" f%d.%d", FormatWidth, FormatPrecision); break;
		case 'E': printf (" e%d.%d", FormatWidth, FormatPrecision); break;
		case 'D': printf (" d%d.%d", FormatWidth, FormatPrecision); break;
		}
	}
	printf ("\n");
	return (0);
}
#endif
