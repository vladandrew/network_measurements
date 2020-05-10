# increase the buffers
sysctl -w net.core.rmem_max=26214400
sysctl -w net.core.rmem_default=26214400

# enable routing
sysctl -w net.ipv4.ip_forward=1
