############################ Image #############################

FROM ubuntu:20.04
ENV TZ="Europe/London"
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime
RUN echo $TZ > /etc/timezone && rm -rf /var/lib/apt/lists/*

############################ General dependencies ############################

RUN apt update -y
RUN apt install -y cmake wget python3 python3-pip python3-dev tar git


############################ Python dependencies ############################
RUN pip3 install scipy>=1.10.0
RUN pip3 install exrex==0.10.5
RUN pip3 install clang==14.0
RUN pip3 install libclang==14.0.6


############################ LLVM ############################
RUN cd $HOME && wget https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz \
    && tar -xf clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz \
    && mv clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04 llvm \
    && rm clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz


############################ Install TACO/PyTACO ############################
RUN cd $HOME && git clone https://github.com/tensor-compiler/taco && cd taco && mkdir -p build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DPYTHON=ON .. && make -j$(nproc) 
RUN echo "export PYTHONPATH=${HOME}/taco/build/lib:$PYTHONPATH" >> ~/.bashrc


############################ Clone C2TACO ############################
RUN cd $HOME && git clone https://github.com/JWesleySM/c2taco
RUN cd ${HOME}/c2taco && ./build_code_analyses.sh ${HOME}/llvm


WORKDIR /root/c2taco
ENTRYPOINT ["/bin/bash"]
