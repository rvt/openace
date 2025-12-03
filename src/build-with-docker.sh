docker build -t gatas-build-image-2.1.1-up .
#docker create --name gatas-builder-container gatas-image
#docker cp gatas-builder-container:/opt/src/pico/build/gatas.uf2 ./gatas.uf2
#docker rm gatas-build-image-2.1.1-up
#docker create --name gatas-builder-container gatas-build-image
#docker container start gatas-builder-container
#docker run -it --entrypoint bash gatas-build-image-2.1.1-up

#DIR=$(pwd)


docker run --rm -e -DPICO_PLATFORM=rp2350 -e -DPICO_BOARD=pico2_w -e -DBUILDER_DIR_NAME=docker_release_rp2350 -v $(pwd):/opt/src --entrypoint /build-entrypoint.sh gatas-build-image-2.1.1-up
