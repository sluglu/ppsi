/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Copyright (C) 2014 GSI (www.gsi.de)
 * Author: Alessandro Rubin
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>

int pp_lib_may_issue_request(struct pp_instance *ppi)
{
	int e;

	if (!pp_timeout(ppi, PP_TO_REQUEST))
		return 0;

	pp_timeout_set(ppi, PP_TO_REQUEST);
	e = msg_issue_request(ppi);
	if (e)
		return e;
	ppi->t3 = ppi->last_snt_time;
	return 0;
}
