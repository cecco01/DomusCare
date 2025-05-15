ip_address = "('fd00::204:4:4:4', 5683)"

# Converti la stringa in una tupla
ip, port = eval(ip_address)

print(ip)   # Output: fd00::204:4:4:4
print(port) # Output: 5683