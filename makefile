RM=rm -rf

all: jarvis

jarvis:
	cmake -S . -B build
	cmake --build build -j$(nproc)

clean:
	${RM} build/
