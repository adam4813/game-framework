// Act 3's script. Captain Crab's split (OnBossPhase phase 2) is announced here; the split itself is C++'s.
void OnBossPhase(const string &in id, int phase)
{
	if (id == "captain_crab" && phase == 2) {
		Banner("THE CRABNADO SPLITS!");
		Sting("split");
	}
}
