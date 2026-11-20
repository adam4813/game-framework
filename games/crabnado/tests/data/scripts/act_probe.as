// The Act script of campaign_scripts.json. Every event it hears is appended to a trail that Banner() then shows, so a
// test reads the whole history back from CampaignScripts::banner. Its globals live as long as the Act.
string trail;
int stagesStarted = 0;

void Note(const string &in line)
{
	trail += line + ";";
	Banner(trail);
}

void OnActStart() { Note("act"); }

void OnStageStart()
{
	stagesStarted++;
	Note("start" + Stage() + "#" + stagesStarted);
	if (Stage() == 1) { Spawn("crab", 3, "crabnado"); }
	// The Boss Stage is the Act's fifth.
	if (Stage() == 5) { SpawnBoss("crabnado"); }
}

void OnStageEnd(const string &in reason) { Note("end:" + reason); }
void OnBossStart(const string &in id) { Note("boss:" + id); }
void OnBossPhase(const string &in id, int phase) { Note(id + ":phase" + phase); }
void OnBossDefeated(const string &in id) { Note("defeated:" + id); }
void OnKill(const string &in type, const string &in source) { Note("kill:" + type + "@" + source); }

// The refusal tests fire these by name through the Remote API.
void OnTimer(const string &in name)
{
	if (name == "refuse_end") { EndStage(); }
	if (name == "refuse_boss") { SpawnBoss("nope"); }
	if (name == "refuse_sting") { Sting("nonsense"); }
	if (name == "refuse_spawn") { Spawn("kraken", 1, "crabnado"); }
	if (name == "sting") { Sting("stage_sting"); }
	if (name == "read") { Note("read:" + Act() + "/" + Stage() + "/" + Kills()); }
}
