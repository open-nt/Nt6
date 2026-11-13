// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qmap.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>

#include <functional>
#include <memory>

class QProcessEnvironment {
public:
    /*!
        \enum QProcessEnvironment::Initialization

        \value InheritFromParent Create a placeholder that makes the child
        process inherit the environment of the parent process.
    */
    enum Initialization { InheritFromParent };

    QProcessEnvironment();
    QProcessEnvironment(Initialization);

    /*!
        Exchanges this environment with \a other.
    */
    void swap(QProcessEnvironment &other) noexcept {
        m_vars.swap(other.m_vars);
        std::swap(m_inherit, other.m_inherit);
    }

    friend bool operator==(const QProcessEnvironment &lhs, const QProcessEnvironment &rhs) {
        return lhs.m_inherit == rhs.m_inherit && lhs.m_vars == rhs.m_vars;
    }
    friend bool operator!=(const QProcessEnvironment &lhs, const QProcessEnvironment &rhs) { return !(lhs == rhs); }

    bool isEmpty() const;
    bool inheritsFromParent() const;
    void clear();
    bool contains(const QString &name) const;
    void insert(const QString &name, const QString &value);
    void remove(const QString &name);
    QString value(const QString &name, const QString &defaultValue = QString()) const;
    QStringList toStringList() const;
    QStringList keys() const;
    void insert(const QProcessEnvironment &e);

    static QProcessEnvironment systemEnvironment();

private:
    QMap<QString, QString> m_vars;
    bool m_inherit = false;
};

class QProcess : public QIODevice {
    Q_OBJECT
public:
    /*!
        \enum QProcess::ProcessError

        This enum describes the errors that can occur.

        \value FailedToStart The process failed to start, for example because the program does not exist or the process lacks permission to run it.
        \value Crashed The process crashed some time after starting successfully.
        \value Timedout The last waitFor...() function timed out. The state of QProcess is unchanged, and you can try calling waitFor...() again.
        \value WriteError An error occurred when attempting to write to the process.
        \value ReadError An error occurred when attempting to read from the process.
        \value UnknownError An unknown error occurred.
    */
    enum ProcessError { FailedToStart, Crashed, Timedout, ReadError, WriteError, UnknownError };

    /*!
        \enum QProcess::ProcessState

        \value NotRunning The process is not running.
        \value Starting The process is starting, but the program has not yet been invoked.
        \value Running The process is running and is ready for reading and writing.
    */
    enum ProcessState { NotRunning, Starting, Running };

    /*!
        \enum QProcess::ProcessChannel

        \value StandardOutput The standard output (stdout) of the running process.
        \value StandardError The standard error (stderr) of the running process.
    */
    enum ProcessChannel { StandardOutput, StandardError };

    /*!
        \enum QProcess::ProcessChannelMode

        \value SeparateChannels QProcess manages the output of the running process, keeping standard output and standard error data in separate internal buffers.
        \value MergedChannels QProcess merges the output of the running process into the standard output channel.
        \value ForwardedChannels QProcess forwards the output of the running process onto the main process: the child writes to the parent's stdout and stderr.
        \value ForwardedOutputChannel Only the standard output of the child is forwarded; standard error is managed.
        \value ForwardedErrorChannel Only the standard error of the child is forwarded; standard output is managed.
    */
    enum ProcessChannelMode {
        SeparateChannels,
        MergedChannels,
        ForwardedChannels,
        ForwardedErrorChannel,
        ForwardedOutputChannel
    };

    /*!
        \enum QProcess::InputChannelMode

        \value ManagedInputChannel QProcess manages the input of the running process.
        \value ForwardedInputChannel The child process reads the standard input of the parent process.
    */
    enum InputChannelMode { ManagedInputChannel, ForwardedInputChannel };

    /*!
        \enum QProcess::ExitStatus

        \value NormalExit The process exited normally.
        \value CrashExit The process crashed, that is, it was terminated by a signal.
    */
    enum ExitStatus { NormalExit, CrashExit };

    explicit QProcess(QObject *parent = nullptr);
    ~QProcess() override;

    void start(const QString &program, const QStringList &arguments = QStringList(), OpenMode mode = ReadWrite);
    void start(OpenMode mode = ReadWrite);
    void startCommand(const QString &command, OpenMode mode = ReadWrite);
    bool startDetached(qint64 *pid = nullptr);
    static bool startDetached(const QString &program, const QStringList &arguments = QStringList(),
                              const QString &workingDirectory = QString(), qint64 *pid = nullptr);
    static int execute(const QString &program, const QStringList &arguments = QStringList());
    static QStringList splitCommand(QStringView command);
    static QStringList systemEnvironment();
    static QString nullDevice();

    bool open(OpenMode mode = ReadWrite) override;
    QString program() const;
    void setProgram(const QString &program);
    QStringList arguments() const;
    void setArguments(const QStringList &arguments);
    QString workingDirectory() const;
    void setWorkingDirectory(const QString &dir);
    QProcessEnvironment processEnvironment() const;
    void setProcessEnvironment(const QProcessEnvironment &environment);

    ProcessChannelMode processChannelMode() const;
    void setProcessChannelMode(ProcessChannelMode mode);
    InputChannelMode inputChannelMode() const;
    void setInputChannelMode(InputChannelMode mode);
    ProcessChannel readChannel() const;
    void setReadChannel(ProcessChannel channel);
    void closeReadChannel(ProcessChannel channel);
    void closeWriteChannel();
    void setStandardInputFile(const QString &fileName);
    void setStandardOutputFile(const QString &fileName, OpenMode mode = Truncate);
    void setStandardErrorFile(const QString &fileName, OpenMode mode = Truncate);
    void setStandardOutputProcess(QProcess *destination);
    void setChildProcessModifier(const std::function<void()> &modifier);
    std::function<void()> childProcessModifier() const;
    [[noreturn]] static void failChildProcessModifier(const char *description, int error = 0) noexcept;

    /*!
        \enum QProcess::UnixProcessFlag
        \since 6.6
        This enum describes what the child process does to itself before it runs the program.

        \value ResetSignalHandlers Signal handlers are reset to their defaults. Nt always does this.
        \value IgnoreSigPipe \c SIGPIPE is ignored rather than reset, so that writing to a closed pipe fails with
            \c EPIPE instead of ending the program.
        \value CloseFileDescriptors The file descriptors from UnixProcessParameters::lowestFileDescriptorToClose
            on (and at least 3) are closed, so that the program does not inherit them.
        \value UseVFork Accepted and ignored: Nt always forks.
        \value CreateNewSession The child starts a new session with \c setsid().
        \value DisconnectControllingTerminal The child leaves the controlling terminal of the parent.
        \value ResetIds The effective user and group ids are set to the real ones, dropping setuid privileges.
        \value DisableCoreDumps The child cannot write core dumps.
    */
    enum class UnixProcessFlag : quint32 {
        ResetSignalHandlers = 0x0001,
        IgnoreSigPipe = 0x0002,
        CloseFileDescriptors = 0x0010,
        UseVFork = 0x0020,
        CreateNewSession = 0x0040,
        DisconnectControllingTerminal = 0x0080,
        ResetIds = 0x0100,
        DisableCoreDumps = 0x0200,
    };
    Q_DECLARE_FLAGS(UnixProcessFlags, UnixProcessFlag)

    /*!
        \class QProcess::UnixProcessParameters
        \since 6.6
        \brief The UnixProcessParameters struct holds what the child process does to itself before it runs the
        program.

        \sa QProcess::setUnixProcessParameters()
    */
    struct UnixProcessParameters {
        /*!
            \variable QProcess::UnixProcessParameters::flags
            What the child does.
        */
        UnixProcessFlags flags = {};
        /*!
            \variable QProcess::UnixProcessParameters::lowestFileDescriptorToClose
            With \c CloseFileDescriptors, the lowest descriptor that is closed; values below 3 count as 3.
        */
        int lowestFileDescriptorToClose = 0;
        /*!
            \variable QProcess::UnixProcessParameters::_reserved
            \internal
        */
        quint32 _reserved[6]{};
    };
    UnixProcessParameters unixProcessParameters() const noexcept;
    void setUnixProcessParameters(const UnixProcessParameters &params);
    void setUnixProcessParameters(UnixProcessFlags flagsOnly);

    QStringList environment() const;
    void setEnvironment(const QStringList &environment);

    ProcessError error() const;
    ProcessState state() const;
    qint64 processId() const;
    int exitCode() const;
    ExitStatus exitStatus() const;

    bool waitForStarted(int msecs = 30000);
    bool waitForReadyRead(int msecs = 30000) override;
    bool waitForBytesWritten(int msecs = 30000) override;
    bool waitForFinished(int msecs = 30000);

    QByteArray readAllStandardOutput();
    QByteArray readAllStandardError();

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    void close() override;

public slots:
    void terminate();
    void kill();

signals:
    /*!
        This signal is emitted by QProcess when the process has started and
        state() returns \c Running.

        \sa errorOccurred(), stateChanged()
    */
    void started() { emitSignal(&QProcess::started); }
    /*!
        This signal is emitted when the process finishes. \a exitCode is the
        exit code of the process (only valid for normal exits), and
        \a exitStatus is the exit status. After the process has finished,
        the buffers in QProcess are still intact. You can still read any
        data that the process may have written before it finished.

        \sa exitStatus(), exitCode(), errorOccurred()
    */
    void finished(int exitCode, QProcess::ExitStatus exitStatus = NormalExit) {
        emitSignal(&QProcess::finished, exitCode, exitStatus);
    }
    /*!
        This signal is emitted when an error occurs with the process. The
        \a error parameter describes the type of error that occurred. It is
        not emitted for \c Timedout.

        \sa error(), errorString()
    */
    void errorOccurred(QProcess::ProcessError error) { emitSignal(&QProcess::errorOccurred, error); }
    /*!
        This signal is emitted whenever the state of QProcess changes. The
        \a newState argument is the state QProcess changed to.

        \sa state()
    */
    void stateChanged(QProcess::ProcessState newState) { emitSignal(&QProcess::stateChanged, newState); }
    /*!
        This signal is emitted when the process has made new data available
        through its standard output channel (stdout). It is emitted
        regardless of the current read channel.

        \sa readAllStandardOutput(), readChannel()
    */
    void readyReadStandardOutput() { emitSignal(&QProcess::readyReadStandardOutput); }
    /*!
        This signal is emitted when the process has made new data available
        through its standard error channel (stderr). It is emitted regardless
        of the current read channel.

        \sa readAllStandardError(), readChannel()
    */
    void readyReadStandardError() { emitSignal(&QProcess::readyReadStandardError); }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    void timerEvent(QTimerEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QProcess::UnixProcessFlags)
