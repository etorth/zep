#include "mainwindow.h"
#include <QPainter>
#include <QCommandLineParser>

#include "buffer.h"
#include "zepwidget_qt.h"


using namespace Zep;

const std::string shader = R"R(
#version 330 core

uniform mat4 Projection;

// Coordinates  of the geometry
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

// Outputs to the pixel shader
out vec2 frag_tex_coord;
out vec4 frag_color;

void main()
{
    gl_Position = Projection * vec4(in_position.xyz, 1.0);
    frag_tex_coord = in_tex_coord;
    frag_color = in_color;
}
)R";

MainWindow::MainWindow()
{

    QCommandLineParser parser;
    parser.setApplicationDescription("Test helper");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", QCoreApplication::translate("file", "file to load."));

    parser.process(*qApp);

    auto* pWidget = new ZepWidget_Qt(this);

    const QStringList args = parser.positionalArguments();
    if (args.size() > 0)
    {
        pWidget->GetEditor().InitWithFileOrDir(args[0].toStdString());
    }
    else
    {
        ZepBuffer* pBuffer = pWidget->GetEditor().AddBuffer("shader.vert");
        pBuffer->SetText("");// shader.c_str());
    }

    setStyleSheet("background-color: darkBlue");

    setContentsMargins(4, 4, 4, 4);
    setCentralWidget(pWidget);
}

