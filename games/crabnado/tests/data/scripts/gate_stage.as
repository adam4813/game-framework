// A Stage script that ends its own `script`-gate Stage after two seconds, through an After() timer.
void OnStageStart() { After(2.0f, "go"); }

void OnTimer(const string &in name)
{
	if (name == "go") { EndStage(); }
}
