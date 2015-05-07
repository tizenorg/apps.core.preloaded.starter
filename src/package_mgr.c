/*
 * Copyright (c) 2000 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vconf.h>
#include <pkgmgr-info.h>

#include "package_mgr.h"
#include "util.h"



bool package_mgr_exist_app(char *appid)
{
	int ret = 0;
	pkgmgrinfo_appinfo_h handle = NULL;

	ret = pkgmgrinfo_appinfo_get_appinfo(appid, &handle);
	if (PMINFO_R_OK != ret || NULL == handle) {
		_SECURE_D("%s doesn't exist in this binary", appid);
		return false;
	}

	pkgmgrinfo_appinfo_destroy_appinfo(handle);

	return true;
}



