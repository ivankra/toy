#!/usr/bin/env python
import math

outer_r = 36
clearance = 0.4
D = [ 18.20, 23.20, 27.40, 23.25, 25.76 ]
A = [ math.asin((d/2+clearance)/(outer_r-d/2-clearance)) * 2 * 180/math.pi for d in D ]
sep = (360 - sum(A)) / len(A)
a = 0
for i in range(len(D)):
    a += A[i]/2.0 + sep + A[(i+1) % len(A)]/2.0
    print '%.6f,' % a,
