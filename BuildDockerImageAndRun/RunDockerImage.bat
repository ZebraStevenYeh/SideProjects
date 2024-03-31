@REM docker run -it steven/amd64_ubuntu20.4 /bin/bash  => this command doesn't have a shared folder between host machine and the container.
@REM "-v C:\Users\top90\Desktop\BuildMyGit:/shared" means to share host machine directory C:\Users\top90\Desktop\BuildMyGit with container directory /shared
docker run -it -v C:\Users\top90\Desktop\BuildMyGit:/shared steven/amd64_ubuntu20.4 /bin/bash