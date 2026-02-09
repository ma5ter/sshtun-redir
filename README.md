# SSHTUN-REDIR

SSHTUN-REDIR is a utility designed to run under inetd/xinetd that manages SSH tunnels. It checks if a tunnel is active
and refreshes it if necessary, then redirects traffic from the client to the SSH tunnel.

## Features

- Automatic SSH tunnel management
- Runs as an inetd/xinetd service
- Simple configuration
- Lightweight and efficient

## Installation

### Prerequisites

- SSH client installed
- inetd or xinetd service manager
- C compiler (for building from source)

### Building from Source

```bash
git clone https://github.com/ma5ter/sshtun-redir.git
cd sshtun-redir
make
```

### Installation

```bash
sudo make install
```

### Preparations

1. **Create a dedicated user** for the SSH tunnel service:
   ```bash
   sudo useradd -r -s /usr/sbin/nologin -m -d /home/sshtun sshtun
   ```
    - `-r`: Creates a system user
    - `-s /usr/sbin/nologin`: Sets shell to nologin (no direct shell access)
    - `-m`: Creates home directory
    - `-d /home/sshtun`: Specifies home directory path


2. **Temporary login** to generate SSH keys:
   ```bash
   sudo su -s /bin/bash sshtun
   ```

3. **Generate SSH key pair** (accept defaults or specify custom path):
   ```bash
   ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519
   ```
    - `-t ed25519`: Uses modern Ed25519 algorithm (recommended)
    - `-f ~/.ssh/id_ed25519`: Specifies key file location


4. **Exit** the temporary session:
   ```bash
   exit
   ```

5. **Copy public key** to the SSH server:

   Append `/home/sshtun/.ssh/id_ed25519.pub` to the server's user `.ssh/authorized_keys`

## Usage

### Command Line

No command line usage needed, only xinetd/inetd should interact with the utility.

### xinetd Configuration

Create a file at `/etc/xinetd.d/` using following example:

```conf
service sshtun-redir
{
    type            = UNLISTED
    port            = 1080
    socket_type     = stream
    protocol        = tcp
    wait            = no
    user            = sshtun
    server          = /usr/local/bin/sshtun-redir
    server_args     = 1080 user@example.com 22
    disable         = no
}
```

Then restart xinetd

### Browser and Web Clients Configuration

#### **Firefox**:
- Go to Settings → Network Settings
- Select "Manual proxy configuration"
- Set SOCKS Host to `localhost` and Port to `1080`
- Select "SOCKS v5"
- Check "Proxy DNS when using SOCKS v5"

#### **Chrome/Edge**:
- Use the `--proxy-server="socks5://localhost:1080"` command line flag
- Or install a proxy extension like "SwitchyOmega"

#### **Safari**:
- Go to System Preferences → Network → Advanced → Proxies
- Enable SOCKS Proxy and enter `localhost:1080`

#### **curl**:
```bash
curl -x socks5h://localhost:1080 https://example.com
```
- `socks5h` means SOCKS5 with remote DNS resolution

#### **wget**:
```bash
wget -e use_proxy=yes -e http_proxy=socks5h://localhost:1080 https://example.com
```

#### **git**:
```bash
git config --global http.proxy socks5h://localhost:1080
```

#### Additional Notes

- Replace `1080` with your configured xinetd port if different
- For system-wide proxy settings, configure `/etc/environment` or your desktop environment's network settings
- Remember to disable the proxy when not using the tunnel

## Documentation

For complete documentation, see the man page:

```bash
man sshtun-redir
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## Support

For support, please open an issue on the GitHub repository.
