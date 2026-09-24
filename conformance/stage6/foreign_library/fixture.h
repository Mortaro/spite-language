#define FIXTURE_ANSWER 7
#define FIXTURE_LIMIT 2
typedef struct { int x; int y; } POINT_PAIR;
int GetAnswerPos(void);
void MovePoint(POINT_PAIR* point);
long long WideSum(long long left, unsigned long long right);
int Scaled(float factor, int value);
long long HandleTwice(void* handle);
void Forget(int value);
