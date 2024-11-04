static char *units [17] = {
	"три", "четыре", "пять", "шесть", "семь", "восемь", "девять", "десять",
	"один", "две", "три", "четыр", "пят", "шест", "сем", "восем", "девят",
};

static char *ones [] = { "один", "одна", "одно", };
static char *twos [] = { "два", "две", "два", };
static char *mills [] = { "миллион", "миллиона", "миллионов", };
static char *thous [] = { "тысяча" , "тысячи", "тысяч", };

static char *decas [8] = {
	"двадцать", "тридцать", "сорок", "пятьдесят",
	"шестьдесят", "семьдесят", "восемьдесят", "девяносто",
};

static char *handrs [9] = {
	"сто", "двести", "триста", "четыреста", "пятьсот",
	"шестьсот", "семьсот", "восемьсот", "девятьсот",
};

static void put (char *x, char **y)
{
	while (*x)
		*(*y)++ = *x++;
}

int StoText (int num, int *mode, char *data)
{
	char* first = data;

	if (num == 0) {
		put ("ноль", &data);
		*mode = 2;              /* ноль мужей */
		return (data - first);
	}
	if (num >= 100) {
		put (handrs[num/100-1], &data);
		if ((num %= 100) != 0)
			*data++ = ' ';
	}
	if (num >= 20) {
		put (decas[num/10-2], &data);
		if ((num %= 10) != 0)
			*data++ = ' ';
	}
	if (num >= 3) {
		put (units[num-3], &data);
		if (num >= 11)
			put ("надцать", &data);
		*mode = (num>=5 ? 2 : 1);       /* 3 мужа / 15 мужей */
	} else if (num == 1) {                  /* 1 муж, 1 баба, 1 окно */
		put (ones[*mode], &data);
		*mode = 0;
	} else if (num == 2) {                  /* 2 мужа, 2 бабы, 2 дерева */
		put (twos[*mode], &data);
		*mode = 1;
	} else                                  /* 40 мужей, 200 баб */
		*mode = 2;
	return (data - first);
}

int LtoText (long num, int *mode, char *data)
{
	char* first = data;
	int mymode;

	if (num == 0)
		return (StoText (0, mode, data));
	if (num >= 1000000) {
		mymode = 0;
		data += StoText ((short) (num / 1000000), &mymode, data);
		*data++ = ' ';
		put (mills [mymode], &data);
		if ((num %= 1000000) != 0)
			*data++ = ' ';
	}
	if (num >= 1000) {
		mymode = 1;
		data += StoText ((short) (num / 1000), &mymode, data);
		*data++ = ' ';
		put (thous[mymode], &data);
		if ((num %= 1000) != 0)
			*data++ = ' ';
	}
	if (num != 0)
		data += StoText ((short) num, mode, data);
	else
		*mode = 2;
	return (data - first);
}

#if 0
static char *who [9] = {
       "мужик", "мужика", "мужиков",
       "баба", "бабы", "баб",
       "дерево", "дерева", "деревьев",
};

main ()
{
	char buffer [200];
	extern long random ();

	for(;;) {
		long data = random ();
		int sex = (unsigned short) random() % 3, mode = sex;
		char *buf = buffer;
		if (data >= 0 && data < 1000000000) {
			int len = LtoText (data, &mode, buffer);
			printf ("%.*s %s\n", len, buffer, who[sex*3+mode]);
		}
	}
}
#endif
