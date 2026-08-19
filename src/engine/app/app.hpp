#pragma once

namespace engine {

class EngineContext;

class App {
public:
	explicit App(EngineContext* context);
	~App();

	void Tick() const;
	void PostTick() const;

private:
	EngineContext* context_;
};

} // namespace engine
