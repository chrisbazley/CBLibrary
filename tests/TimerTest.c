/*
 * CBLibrary test: Timer
 * Copyright (C) 2016 Christopher Bazley
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ISO library headers */
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

/* CBLibrary headers */
#include "Timer.h"
#include "Macros.h"

/* Local headers */
#include "Tests.h"

enum
{
  WaitTime = 500, /* centiseconds */
  AcceptableDelay = 1, /* centiseconds */
  NFlags = 3
};

static void test1(void)
{
  /* Register and wait */
  bool timeup_flag = true;
  const clock_t start = clock();
  _Optional CONST _kernel_oserror * const e = timer_register(&timeup_flag, WaitTime);
  assert(e == NULL);
  assert(!timeup_flag);

  clock_t elapsed = 0;
  do
  {
    elapsed = ((clock() - start) * 100) / CLOCKS_PER_SEC;
    printf("Waiting %u\n", (unsigned int)elapsed);

    if (elapsed < WaitTime)
    {
      assert(!timeup_flag);
    }
  }
  while (elapsed <= WaitTime + AcceptableDelay);

  assert(timeup_flag);
}

static void test2(void)
{
  /* Register and deregister */
  bool timeup_flag = true;
  const clock_t start = clock();
  _Optional CONST _kernel_oserror *e = timer_register(&timeup_flag, WaitTime);
  assert(e == NULL);
  assert(!timeup_flag);

  e = timer_deregister(&timeup_flag);
  assert(e == NULL);
  assert(!timeup_flag);

  clock_t elapsed = 0;
  do
  {
    elapsed = ((clock() - start) * 100) / CLOCKS_PER_SEC;
    printf("Waiting %u\n", (unsigned int)elapsed);
  }
  while (elapsed < (WaitTime*2));

  assert(!timeup_flag);
}

static void test3(void)
{
  /* Register multiple and wait */
  bool timeup_flags[NFlags];
  _Optional CONST _kernel_oserror *e = NULL;

  const clock_t start = clock();
  for (size_t n = 0; n < NFlags; ++n)
  {
    timeup_flags[n] = true;
    e = timer_register(&timeup_flags[n], WaitTime*(n+1));
    assert(e == NULL);
    assert(!timeup_flags[n]);
  }

  clock_t elapsed = 0;
  do
  {
    elapsed = ((clock() - start) * 100) / CLOCKS_PER_SEC;
    printf("Waiting %u\n", (unsigned int)elapsed);
    for (size_t n = 0; n < NFlags; ++n)
    {
      if (elapsed < (clock_t)(WaitTime*(n+1)))
      {
        assert(!timeup_flags[n]);
      }
      else if (elapsed > (clock_t)(WaitTime*(n+1) + AcceptableDelay))
      {
        assert(timeup_flags[n]);
      }
    }
  }
  while (elapsed < (WaitTime*(NFlags+1)));

  for (size_t n = 0; n < NFlags; ++n)
  {
    assert(timeup_flags[n]);
  }
}

static void test4(void)
{
  /* Register and deregister multiple and wait */
  bool timeup_flags[NFlags];
  _Optional CONST _kernel_oserror *e = NULL;

  const clock_t start = clock();
  for (size_t n = 0; n < NFlags; ++n)
  {
    timeup_flags[n] = true;
    e = timer_register(&timeup_flags[n], WaitTime*(n+1));
    assert(e == NULL);
    assert(!timeup_flags[n]);
  }

  for (size_t n = 0; n < NFlags; ++n)
  {
    if (n % 2)
    {
      e = timer_deregister(&timeup_flags[n]);
      assert(e == NULL);
      assert(!timeup_flags[n]);
    }
  }

  clock_t elapsed = 0;
  do
  {
    elapsed = ((clock() - start) * 100) / CLOCKS_PER_SEC;
    printf("Waiting %u\n", (unsigned int)elapsed);
    for (size_t n = 0; n < NFlags; ++n)
    {
      /* If we deregistered the timer then the flag should always be false */
      if ((elapsed < (clock_t)(WaitTime*(n+1))) || (n % 2))
      {
        assert(!timeup_flags[n]);
      }
      else if (elapsed > (clock_t)(WaitTime*(n+1) + AcceptableDelay))
      {
        assert(timeup_flags[n]);
      }
    }
  }
  while (elapsed < (clock_t)(WaitTime*(NFlags+1)));

  for (size_t n = 0; n < NFlags; ++n)
  {
    /* If we didn't deregister the timer then the flag should be true */
    if (!(n % 2))
    {
      assert(timeup_flags[n]);
    }
  }
}

void Timer_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Register and wait", test1 },
    { "Register and deregister", test2 },
    { "Register multiple and wait", test3 },
    { "Register and deregister multiple and wait", test4 }
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); count ++)
  {
    printf("Test %zu/%zu : %s\n",
           1 + count,
           ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);

    unit_tests[count].test_func();
  }
}
