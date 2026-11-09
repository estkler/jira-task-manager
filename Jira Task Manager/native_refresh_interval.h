#ifndef NATIVE_REFRESH_INTERVAL_H
#define NATIVE_REFRESH_INTERVAL_H
/* Clamp legacy settings before rounding, avoiding overflow even at INT_MAX.
   Integer minutes have no midpoint ties: 17 -> 15, 18 -> 20. */
static int NativeRefreshIntervalNormalize(int minutes)
{
    if(minutes<15)return 15;
    if(minutes>45)return 45;
    return ((minutes+2)/5)*5;
}
#endif
