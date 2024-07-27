#include <signal.h>  // for signal, sigaction, raise, sigemptyset, SIG_IGN

static void linux_signal_handler(int signum)
{
	// Resume default behavior for the signal to exit without calling back signalHandler()
	// Raise it to get a core, with gdb pointing directly at the right thread, and also return the right exit code.
	signal(signum, SIG_DFL);
	raise(signum);
}

// 监听系统信号处理
// 主要是LINUX系统下的信号监听
void RegisterSystemSignalHandler()
{
	/* Set up the structure to specify the new action. */
	struct sigaction action_;
	action_.sa_handler = linux_signal_handler;
	sigemptyset(&action_.sa_mask);
	action_.sa_flags = SA_ONSTACK; // Use dedicated alternate signal stack

	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	//sigaction(SIGINT, &action_, NULL); // 2
	//sigaction(SIGQUIT, &action_, NULL); // 3
	//sigaction(SIGILL, &action_, NULL); // 4
	//sigaction(SIGTRAP, &action_, NULL); // 5
	//sigaction(SIGABRT, &action_, NULL); // 6
	//sigaction(SIGFPE, &action_, NULL); // 8
	//sigaction(SIGSEGV, &action_, NULL); // 11
	//sigaction(SIGTERM, &action_, NULL); // 15
}

