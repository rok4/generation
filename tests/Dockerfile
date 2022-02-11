FROM rok4/rok4generation:4.0.0-RC-debian11

RUN mkdir /tests
WORKDIR /tests

RUN apt -y install git && git init && git submodule add https://github.com/bats-core/bats-core.git bats \
    && git submodule add https://github.com/bats-core/bats-support.git test_helper/bats-support \
    && git submodule add https://github.com/bats-core/bats-assert.git test_helper/bats-assert \
    && git submodule add https://github.com/bats-core/bats-file.git test_helper/bats-file

COPY inputs /inputs
COPY suite/* ./

CMD ["./bats/bin/bats", "-T", "--print-output-on-failure", "."]