// Run under inetd/xinetd

#define WAIT_TIMEOUT (10)
#define SSH_BINARY "/usr/bin/ssh"
#define RUN_DIR "/run/ssh-tunnel/"
#define LOG_FILE "/var/log/ssh-tunnel/redir.log"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

static void die(const char *msg, const int code) {
	fprintf(stderr, "%s\n", msg);
	_exit(code);
}

/// \brief Checks if a name string contains only safe characters.
///
/// \param[in] s The input string to validate.
///
/// \return true if the string contains only allowed characters (A-Z, a-z, 0-9, '_', '-', '.', '@').
/// \return false if the string contains any disallowed characters or is NULL.
static bool is_safe_name(const char *s) {
	// allow: A-Z a-z 0-9 _ - . @
	for (; *s; s++) {
		const char c = *s;
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '@')) {
			return false;
		}
	}
	return true;
}

/// \brief Checks if an SSH tunnel is currently active by querying the SSH master connection.
///
/// \param[in] ssh_user_host The SSH user@host string identifying the tunnel.
/// \param[in] ssh_port The SSH port number for the tunnel connection.
///
/// \return true if the SSH master connection is active (exit status 0).
/// \return false if the SSH master connection is not active or an error occurred.
static bool is_tunnel_active(const char *ssh_user_host, const uint16_t ssh_port) {
	char string[1024];

	// Check whether a master connection is alive; ssh -O check returns non-zero if not.
	snprintf(string, sizeof(string),
	SSH_BINARY " -p %u -S '" RUN_DIR "%s:%u.ctl' -O check '%s' >/dev/null 2>&1",
	ssh_port, ssh_user_host, ssh_port, ssh_user_host);

	const int st = system(string);
	if (st == -1) die("exec check error", 21);

	return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

static void refresh_tunnel(const uint16_t port, const char *ssh_user_host, const uint16_t ssh_port) {
	const char *msg = nullptr;
	int code = 0;
	char string[1024];

	// Create a lock file path based on the control socket path
	snprintf(string, sizeof(string), RUN_DIR "%s:%u.lock", ssh_user_host, ssh_port);
	const int lock_fd = open(string, O_CREAT | O_RDWR, 0600);
	if (lock_fd < 0) die("failed to open lock file", 22);

	// Acquire exclusive lock
	struct flock fl = {0};
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	if (fcntl(lock_fd, F_SETLKW, &fl) < 0) {
		close(lock_fd);
		die("failed to acquire lock", 23);
	}

	// If check failed, start (or restart) the ControlMaster with a local SOCKS proxy on 127.0.0.1:port.
	if (!is_tunnel_active(ssh_user_host, ssh_port)) {
		snprintf(string, sizeof(string), SSH_BINARY
		" -o ExitOnForwardFailure=yes"
		" -o ControlMaster=yes"
		" -o ControlPath='" RUN_DIR "%s:%u.ctl'"
		" -o ControlPersist=10m"
		" -o ServerAliveInterval=30"
		" -o ServerAliveCountMax=2"
		" -p %u -N -f -D 127.0.0.1:%u"
		" '%s' >>'" LOG_FILE "' 2>&1",
		ssh_user_host, ssh_port, ssh_port, port, ssh_user_host);

		const int st = system(string);
		if (st == -1) {
			msg = "exec start error";
			code = 24;
		}
		else if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0)) {
			msg = "tunnel start error";
			code = 25;
		}
		else {
			msg = "tunnel ready timeout";
			code = 26;
			for (int i = 0; i < WAIT_TIMEOUT; ++i) {
				sleep(1);
				if (is_tunnel_active(ssh_user_host, ssh_port)) {
					msg = nullptr;
					code = 0;
					break;
				}
			}
		}
	}

	fl.l_type = F_UNLCK;
	fcntl(lock_fd, F_SETLK, &fl);
	close(lock_fd);

	if (msg && code) die(msg, code);
}

int main(const int argc, char **argv) {
	if (argc < 3) die("usage: sshtun-redir <local-port> <ssh-user-host> [ssh-port]", 2);

	const char *ssh_user_host = argv[2];
	if (!ssh_user_host[0] || !is_safe_name(ssh_user_host)) die("invalid ssh-user-host", 3);

	char *end = nullptr;
	long port_l = strtol(argv[1], &end, 10);
	if (!end || *end || port_l <= 0 || port_l > 65535) die("invalid port", 4);
	const uint16_t port = (uint16_t)port_l;

	uint16_t ssh_port = 22;
	if (argc == 4) {
		end = nullptr;
		port_l = strtol(argv[3], &end, 10);
		if (!end || *end || port_l <= 0 || port_l > 65535) die("invalid ssh port", 5);
		ssh_port = (uint16_t)port_l;
	}

	refresh_tunnel(port, ssh_user_host, ssh_port);

	const int out = socket(AF_INET, SOCK_STREAM, 0);
	if (out < 0) die("socket error", 11);

	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

	if (connect(out, (struct sockaddr *)&sa, sizeof(sa)) < 0) die("connect error", 12);

	// inetd provides the already-accepted client socket on fd 0/1/2
	struct pollfd pfd[2];
	unsigned char buf[8192];
	int in_open = 1, out_open = 1;

	while (in_open || out_open) {
		constexpr int in = 0;
		pfd[0].fd = in;
		pfd[0].events = in_open ? POLLIN : 0;
		pfd[1].fd = out;
		pfd[1].events = out_open ? POLLIN : 0;

		const int rc = poll(pfd, 2, -1);
		if (rc < 0) {
			if (errno == EINTR) continue;
			die("poll error", 13);
		}

		// client -> target
		if (in_open && pfd[0].revents & (POLLIN | POLLHUP | POLLERR)) {
			const ssize_t n = read(in, buf, sizeof(buf));
			if (n <= 0) {
				in_open = 0;
				shutdown(out, SHUT_WR);
			}
			else {
				ssize_t off = 0;
				while (off < n) {
					const ssize_t w = write(out, buf + off, (size_t)(n - off));
					if (w < 0) die("write to target error", 14);
					off += w;
				}
			}
		}

		// target -> client
		if (out_open && pfd[1].revents & (POLLIN | POLLHUP | POLLERR)) {
			const ssize_t n = read(out, buf, sizeof(buf));
			if (n <= 0) {
				out_open = 0;
				shutdown(in, SHUT_WR); // may fail on fd 0, ignore
			}
			else {
				ssize_t off = 0;
				while (off < n) {
					const ssize_t w = write(1, buf + off, (size_t)(n - off));
					if (w < 0) die("write to client error", 15);
					off += w;
				}
			}
		}
	}

	close(out);
	return 0;
}
