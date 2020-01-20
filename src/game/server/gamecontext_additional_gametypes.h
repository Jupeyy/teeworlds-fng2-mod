#ifdef CONTEXT_INIT_WITHOUT_CONFIG
/*if (str_comp(m_Config->m_SvGametype, "yourmod") == 0)
	m_pController = new CGameControllerYourMod(this);
else */
	
#else
/*if (str_comp(m_Config->m_SvGametype, "yourmod") == 0)
	m_pController = new CGameControllerYourMod(this, *pConfig);
else*/
#endif