gwan-redis-async
================

How to send commands to redis from G-WAN asynchronously

Usage:
  - Install libev and hiredis
  - copy main.c to 'handlers' folder
  - copy light-lock.h to 'include' folder
  - copy async-redis.c to 'csp' folder
  - You're done! Open this url in your browser http://[your-gwan-host]:[port]/?async-redis.c
