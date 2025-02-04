#ifndef MACROS_H
#define MACROS_H

#define SIGN(x) (((x) > 0) - ((x) < 0))
#define DEGREE_TO_RADIAN(x) (x * 3.14159f) / 180.f;
#define DISTANCE_2D(x_1,y_1,x_2,y_2) sqrt(pow((x_2 - x_1), 2) + pow((y_2 - y_1), 2));

#endif