extern int FormatWidth;                 /* Текущая ширина поля */
extern int FormatPrecision;             /* Текущая точность */
extern int FormatScale;                 /* Масштабный коэффициент */
extern char *FormatText;                /* Указатель на текстовое поле */

extern void FormatInit (char *format);
extern int FormatNext (void);
