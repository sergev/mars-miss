#include "mars.h"
#include "intern.h"

/*
 * Завершение транзакции.  Если флаг не 0,
 * производится подтверждение транзакции (commit),
 * иначе откат (rollback).
 */
void DBcommit (int flag)
{
	_DBio (flag ? 'C' : 'R', 0, 1);
}
