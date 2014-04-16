publicHeaders(root, \
    include/de/Action \
    include/de/Animation \
    include/de/AnimationVector \
    include/de/ConstantRule \
    include/de/IndirectRule \
    include/de/OperatorRule \
    include/de/Rule \
    include/de/RuleBank \
    include/de/RuleRectangle \
    include/de/RootWidget \
    include/de/ScalarRule \
    include/de/Widget \
)

publicHeaders(widgets, \
    include/de/widgets/action.h \
    include/de/widgets/animation.h \
    include/de/widgets/animationvector.h \
    include/de/widgets/constantrule.h \
    include/de/widgets/indirectrule.h \
    include/de/widgets/operatorrule.h \
    include/de/widgets/rulerectangle.h \
    include/de/widgets/rootwidget.h \
    include/de/widgets/rule.h \
    include/de/widgets/rulebank.h \
    include/de/widgets/rules.h \
    include/de/widgets/scalarrule.h \
    include/de/widgets/widget.h \
)

SOURCES += \
    src/widgets/action.cpp \
    src/widgets/animation.cpp \
    src/widgets/constantrule.cpp \
    src/widgets/indirectrule.cpp \
    src/widgets/operatorrule.cpp \
    src/widgets/rulerectangle.cpp \
    src/widgets/rootwidget.cpp \
    src/widgets/rule.cpp \
    src/widgets/rulebank.cpp \
    src/widgets/scalarrule.cpp \
    src/widgets/widget.cpp
