#include <tst/application.hpp>

#include "../../src/httpclient/init_guard.hpp"

namespace{
class application : public tst::application{
    httpclient::init_guard httpc_init_guard;
public:
    application() :
            tst::application(std::string(), std::string())
    {}
};
}

const tst::application_factory app_fac([](){
    return std::make_unique<application>();
});
