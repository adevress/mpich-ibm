<?xml version='1.0' ?>
<xsl:stylesheet  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:template match="/">
<html>
    <head>
        <title>MPICH2 Error Report</title>
        <style type="text/css">
            table         { border-collapse:collapse; }
            table, th, td { border:2px solid blue; }
            td            { vertical-align:top; padding:2px; }
            th            { background-color:#bbf; color:white; }
            tr.fail td    { background-color:#fbb; }
            tr.pass td    { background-color:#bfb; }
        </style>
    </head>
    <body>
        <h1>MPICH2 Error Report</h1>
        <xsl:apply-templates select="MPITESTRESULTS"/>
    </body>
</html>
</xsl:template>

<xsl:template match="MPITESTRESULTS">
    <p>
        <xsl:apply-templates select="DATE"/>
        <xsl:apply-templates select="TOTALTIME"/>.
    </p>

    <table>
        <tr><th>Dir</th><th>Name</th><th>np</th><th>Status</th><th>Time</th><th>Diff</th></tr>
        <xsl:apply-templates select="MPITEST"/>
    </table>
</xsl:template>

<xsl:template match="DATE">Test started at <xsl:value-of select="."/></xsl:template>
<xsl:template match="TOTALTIME"> and ran for <xsl:value-of select="."/> seconds</xsl:template>

<xsl:template match="MPITEST">
    <xsl:variable name="status">
        <xsl:choose>
            <xsl:when test="STATUS = 'pass'">
                <xsl:value-of select="'pass'"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="'fail'"/>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:variable>

    <tr class="{$status}">
    <td><xsl:value-of select="WORKDIR"/></td>
    <td><xsl:value-of select="NAME"/></td>
    <td><xsl:value-of select="NP"/></td>
    <td><xsl:value-of select="STATUS"/></td>
    <td><xsl:value-of select="RUNTIME"/></td>
    <td><pre><xsl:value-of select="TESTDIFF"/></pre></td>
    </tr>
</xsl:template>

<xsl:template match="TRACEBACK">
    <a>
    <xsl:attribute name="HREF">
    <xsl:value-of select="."/>
    </xsl:attribute>
    Traceback
    </a>
</xsl:template>


</xsl:stylesheet>
