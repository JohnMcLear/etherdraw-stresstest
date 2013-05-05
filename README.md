# INSTALLATION AND GETTING STARTED

### 1. Install prerequisites

APT based (Debian/Ubuntu):
`apt-get install libqt4-dev libqjson-dev gdb`

YUM based (CentOS):
`yum install libqt4-dev libqjson-dev gdb`

### 2. Build the binary
`make`

### 3. Test (Assuming Etherdraw is running locally)
`./etherdraw-stresstest http://127.0.0.1:3000/d/foo`

# Examples:
Basic run against localhost drawing id foo: 
`./etherdraw-stresstest http://localhost:3000/d/foo`

Run with 10 lurking clients and 50 drawers:
`./etherdraw-stresstest --clients=lurk:10,draw:50 http://localhost:3000/d/foo`

Run with verbosity/debug output set to the lowest setting: 
`./etherdraw-stresstest --verbosity=0 http://localhost:3000/d/foo`

Run with a username and password set: 
`./etherdraw-stresstest --user=John http://localhost:3000/d/foo`

Run for 3 seconds:
`./etherdraw-stresstest --duration=3 http://localhost:3000/d/foo`


# Options:
`  --clients = JSON PAIR - Type of Client:Number of Clients IE lurk:10`

`  --duration = INTEGER - The duration to run the test for in seconds IE 10`

`  --verbosity = INTEGER - The verbosity of the output to the CLI (0 being lowest) IE 0`

`  --user = STRING - The username used to connect to a pad (You will be prompted for a password) IE john`


