#pragma once

//Runs the pure-HDBSCAN* algorithm unit tests (no Amalgam dependencies).  The tests are
//compiled into the lib_smoke_test driver rather than a standalone executable; this entry
//point lets that driver invoke them.  Prints any failures and a summary line; returns the
//number of failed checks (0 on success).
int RunHDBSCANUnitTests();
