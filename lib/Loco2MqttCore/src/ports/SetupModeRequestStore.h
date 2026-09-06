#pragma once

class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;
    virtual void request() = 0;
    virtual bool consumeIfRequested() = 0;
};
