Part C — ThreadForce Ops (Bakery simulation)
-------------------------------------------

This directory contains a multithreaded simulation of the bakery
scenario described in the assignment. The program models customers and
chefs as threads and enforces capacity, sofa seating, baking, and a
single cash register for payment.

Problem summary
---------------
- Shop capacity: 25 customers total. A customer refuses to enter if
  the shop is at capacity.
- Sofa seats: up to 4 customers may sit. Additional customers stand in
  the shop waiting area.
- Chefs: 4 chef threads (IDs 1..4). Up to 4 customers can have cakes
  baked concurrently.
- Ordering flow for each customer: enterofficebakery -> sitOnSofa ->
  getcake -> pay -> leave.
- Chefs prioritize accepting payments over baking when both tasks are
  available.

Timing and actions
------------------
- Each customer action (enter, sit, request cake, pay, leave) takes
  1 second.
- Each chef action (bake, accept payment) takes 2 seconds and cannot
  be preempted.

COMPILE : gcc -Wall -Wextra -O2 -o bakery bakery.c -lpthread

Build
-----

Compile the simulation in this folder. If a Makefile is provided, run:

COMPILE : gcc -Wall -Wextra -O2 -o bakery bakery.c -lpthread

This will produce an executable (name depends on the provided sources).

Run
---

The program accepts input lines on stdin describing arriving
customers, one per line with the format:

<time_stamp> Customer <id>

Example:

10 Customer 1
11 Customer 2
<EOF>

You can run the program by piping a file or typing input and then
Ctrl+D to signal EOF:

./bakery < arrivals.txt

Output
------

Each action should be printed in chronological order with the format:

<time_stamp> <Customer/Chef> <id> <action>

Example actions for customers: enters, sits, requests cake, pays,
leaves. Example actions for chefs: bakes for Customer X, accepts
payment for Customer X.

Notes and testing
-----------------
- Use proper synchronization primitives (mutexes, condition variables,
  semaphores) to ensure the ordering and constraints described above.
- Make sure to honor the non-preemptive timing: once an action starts,
  it must run for its full duration.

Academic honesty & LLM use
-------------------------
If any code or text was produced using an LLM, follow the course rules
for disclosure and saving prompts/responses.
# Fighting for the resources