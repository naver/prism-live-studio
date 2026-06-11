#ifndef SOURCEINFO_H
#define SOURCEINFO_H

#include <obs-frontend-api.h>
#include <QHash>

class SourceInfo {
public:
	obs_source_t *source;
	std::string name;
	std::string idStr;
	obs_source_type type;
	bool isAudio;
	bool isMuted;
	bool isGroup;

	bool operator==(const SourceInfo &other) const
	{
		auto hash = qHash(name.c_str());
		auto otherHash = qHash(other.name.c_str());
		return hash == otherHash;
	}
};

#endif // SOURCEINFO_H
