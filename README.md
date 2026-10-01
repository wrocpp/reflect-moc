# reflect-moc

A Qt `moc` replacement built on C++26 reflection (P2996, P3394 annotations),
targeting GCC 16.2.

Status: feasibility spikes only. Nothing is designed yet; `spikes/SPIKES.md`
records what the design can rely on.

## Spikes

```sh
spikes/run.sh                      # spikes 01-05 on GCC 16.2 (g++-16)
python3 spikes/ce-clang.py spikes/01-define-aggregate.cpp   # clang-p2996 on Compiler Explorer
docker build -t reflect-moc/gcc16-qt610 docker
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 sh spikes/06-qt/build.sh
```

## License

MIT, see `LICENSE`.
