# increase the buffers
sudo sysctl -w net.core.rmem_max=26214400
sudo sysctl -w net.core.rmem_default=26214400

# enable routing
sudo sysctl -w net.ipv4.ip_forward=0
