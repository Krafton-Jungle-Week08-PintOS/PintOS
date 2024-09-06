/* Tries to read from an invalid fd,
   which must either fail silently or terminate the process with
   exit code -1. */

#include <limits.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

/*
유효하지 않은 파일 디스크립터(fd)로 읽기를 시도해서 발생하는 오류를 처리하거나 exit code -1과 함께 프로세스를 종료하는 것이 목표.

*/
void
test_main (void) 
{
  char buf; // 1바이트 크기의 버퍼를 선언. read 함수에서 데이터 저장 시에 사용
  read (0x20101234, &buf, 1);
  read (5, &buf, 1);
  read (1234, &buf, 1);
  read (-1, &buf, 1);
  read (-1024, &buf, 1);
  read (INT_MIN, &buf, 1);
  read (INT_MAX, &buf, 1);
}
