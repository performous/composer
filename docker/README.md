This folder contains Dockerfiles which will install all dependencies needed to build [Composer](https://github.com/performous/composer/wiki/Building-and-installing-from-source).  

These containers are to be used as `base images` to provide higher-level builds and packages and to produce artifacts for downstream consumption. These containers **do not** provide a running version of `Composer` or contain the project source in any usable form.  

These containers are built automatically during our CI/CD workflow, and are used to support Linux Distros that are no available by default from Github Actions.  

## Building containers
The build is a pretty standard `docker build`, just make sure you explicitly call out a `Dockerfile` with `-f Dockerfile.<distro>` and supply the correct distro version as a `build-arg`:  
```sh
docker build -t composer-docker-build:ubuntu20.04 -f Dockerfile.ubuntu --build-arg OS_VERSION=20.04 .
```

Currently supported distros are:
- Ubuntu (20.04, 22.04)
- Fedora (34, 35, 36)
- Debian (10, 11)

## Running the containers
Once the `base-image` has been built, the container can be run interactively to build `Composer`:  
```sh
docker run -it composer-docker-build:ubuntu20.04
```  

From there, you can [follow the build instructions](https://github.com/performous/composer/wiki/Building-and-installing-from-source#downloading-and-installing-the-sources) to build composer.  


`build_composer.sh` is included in the containers for testing builds and creating OS packages.
```
Usage: ./build_composer.sh -a (build with all build systems)

Optional Arguments:
  -b <Git Branch>: Build the specified git branch, tag, or sha
  -p <Pull Request #>: Build the specified Github Pull Request number
  -g : Generate Packages
  -r <Repository URL>: Git repository to pull from
  -R : Perform a 'Release' Cmake Build (Default is 'RelWithDebInfo')
  -h : Show this help message
```

To build a pull request using just cmake:
```
docker run composer-docker-build:ubuntu20.04 ./build_composer.sh -c -p 626
```

One-Liner to generate packages and copy them to `/tmp` on the running system:
```
mkdir /tmp/composer-packages && docker run --rm --mount type=bind,source=/tmp/composer-packages,target=/composer/packages composer-docker-build:ubuntu20.04 /bin/bash -c './build_composer.sh -c -R -g && cp composer/build/*.deb /composer/packages'
```
