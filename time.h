#ifndef TIME_H_
#define TIME_H_

#include <time.h>
#define NANOSECONDS_IN_SECOND 1000000000
#define MILLISECONDS_IN_SECOND 1000

typedef struct {
	struct timespec absolute;
} AbsoluteTime;

typedef struct {
	struct timespec relative;
} RelativeTime;

__attribute__((unused)) inline static void
timespec_assign_nanosecond(struct timespec *target, long long ns)
{
	time_t sec = ns / NANOSECONDS_IN_SECOND;
	if (ns < 0) {
		sec -= 1;
	}
	target->tv_sec = sec;
	target->tv_nsec = ns - sec * NANOSECONDS_IN_SECOND;
}

__attribute__((unused)) inline static void
timespec_assign_millisecond(struct timespec *target, long long ms)
{
	time_t sec = ms / MILLISECONDS_IN_SECOND;
	if (ms < 0) {
		sec -= 1;
	}
	ms -= sec * MILLISECONDS_IN_SECOND;
	target->tv_sec = sec;
	target->tv_nsec = ms * (NANOSECONDS_IN_SECOND / MILLISECONDS_IN_SECOND);
}

__attribute__((unused)) inline static void
timespec_add(struct timespec *accum, const struct timespec *item)
{
	accum->tv_sec += item->tv_sec;
	accum->tv_nsec += item->tv_nsec;
	while (accum->tv_nsec >= NANOSECONDS_IN_SECOND) {
		++(accum->tv_sec);
		accum->tv_nsec -= NANOSECONDS_IN_SECOND;
	}
}

__attribute__((unused)) inline static void
timespec_sub(struct timespec *accum, const struct timespec *item)
{
	accum->tv_sec -= item->tv_sec;
	accum->tv_nsec -= item->tv_nsec;
	while (accum->tv_nsec < 0) {
		--(accum->tv_sec);
		accum->tv_nsec += NANOSECONDS_IN_SECOND;
	}
}

__attribute__((unused)) inline static int
timespec_cmp(const struct timespec *lhs, const struct timespec *rhs)
{
	if (lhs->tv_sec > rhs->tv_sec) {
		return 1;
	}
	if (lhs->tv_sec < rhs->tv_sec) {
		return -1;
	}
	if (lhs->tv_nsec > rhs->tv_nsec) {
		return 1;
	}
	if (lhs->tv_nsec < rhs->tv_nsec) {
		return -1;
	}
	return 0;
}

__attribute__((unused)) inline static AbsoluteTime
get_current_time()
{
	AbsoluteTime time;
	if (clock_gettime(CLOCK_MONOTONIC, &time.absolute) < 0) {
		time.absolute.tv_sec = 0;
		time.absolute.tv_nsec = 0;
	}
	return time;
}

__attribute__((unused)) inline static AbsoluteTime
absolute_time_add_relative(AbsoluteTime lhs, RelativeTime rhs)
{
	timespec_add(&lhs.absolute, &rhs.relative);
	return lhs;
}

__attribute__((unused)) inline static AbsoluteTime
absolute_time_sub_relative(AbsoluteTime lhs, RelativeTime rhs)
{
	timespec_sub(&lhs.absolute, &rhs.relative);
	return lhs;
}

__attribute__((unused)) inline static RelativeTime
absolute_time_sub_absolute(AbsoluteTime lhs, AbsoluteTime rhs)
{
	RelativeTime result = {
		.relative = lhs.absolute,
	};
	timespec_sub(&result.relative, &rhs.absolute);
	return result;
}

__attribute__((unused)) inline static RelativeTime
relative_time_from_nanosecond(long long ns)
{
	RelativeTime result;
	timespec_assign_nanosecond(&result.relative, ns);
	return result;
}

__attribute__((unused)) inline static RelativeTime
relative_time_from_millisecond(long long ms)
{
	RelativeTime result;
	timespec_assign_millisecond(&result.relative, ms);
	return result;
}

__attribute__((unused)) inline static RelativeTime
relative_time_add(RelativeTime lhs, RelativeTime rhs)
{
	timespec_add(&lhs.relative, &rhs.relative);
	return lhs;
}

__attribute__((unused)) inline static RelativeTime
relative_time_sub(RelativeTime lhs, RelativeTime rhs)
{
	timespec_sub(&lhs.relative, &rhs.relative);
	return lhs;
}

__attribute__((unused)) inline static int
absolute_time_cmp(AbsoluteTime lhs, AbsoluteTime rhs)
{
	return timespec_cmp(&lhs.absolute, &rhs.absolute);
}

__attribute__((unused)) inline static int
relative_time_cmp(RelativeTime lhs, RelativeTime rhs)
{
	return timespec_cmp(&lhs.relative, &rhs.relative);
}

#endif /* end of include guard: TIME_H_ */
